#include "parser.hpp"
#include <iostream>
#include <functional>
#include <sstream>
#include <crails/utils/split.hpp>
#include <crails/utils/join.hpp>
#include <crails/utils/semantics.hpp>

using namespace std;

ostream& operator<<(ostream& stream, const CXString& str)
{
  stream << clang_getCString(str);
  clang_disposeString(str);
  return stream;
}

string cxStringToStdString(const CXString& source)
{
  string result(clang_getCString(source));
  clang_disposeString(source);
  return result;
}

// Edited from Khuck's response in
// https://stackoverflow.com/questions/62005698/libclang-clang-getargtype-returns-wrong-type
static bool find_parsing_errors(CXTranslationUnit translationUnit)
{
  int nbDiag = clang_getNumDiagnostics(translationUnit);
  bool foundError = false;

  if (nbDiag)
    cerr << "There are " << nbDiag << " diagnostics:" << endl;
  for (unsigned int currentDiag = 0 ; currentDiag < nbDiag ; ++currentDiag)
  {
    CXDiagnostic diagnotic = clang_getDiagnostic(translationUnit, currentDiag);
    string tmp = cxStringToStdString(
      clang_formatDiagnostic(diagnotic, clang_defaultDiagnosticDisplayOptions())
    );

    if (tmp.find("error:") != string::npos)
      foundError = true;
    cerr << tmp << endl;
  }
  return foundError;
}

static void twilog(const std::stringstream& stream)
{
  cout << '\r' << stream.str() << endl;
}

#define TWILOG(body) twilog(std::stringstream() << body)

TwiliParser* parser = nullptr;

TwiliParser::TwiliParser()
{
  parser = this;
}

TwiliParser::~TwiliParser()
{
  parser = nullptr;
}

void TwiliParser::print_state()
{
  static const vector<char> cursors{'\\', '|', '/', '-'};
  static int pos = 0;
  string cursor_view("\r");

  if (pos >= cursors.size())
    pos = 0;
  cursor_view += cursors[pos++];
  cout << cursor_view << " parsing... found "
       << types.size() << " types, "
       << classes.size() << " objects, "
       << enums.size() << " enums, "
       << functions.size() << " functions";
  flush(cout);
}

void TwiliParser::add_directory(const string& path)
{
  add_directory(filesystem::path(path));
}

void TwiliParser::add_directory(const filesystem::path& path)
{
  directories.push_back(filesystem::canonical(path).string());
}

filesystem::path TwiliParser::get_current_path() const
{
  CXSourceLocation location = clang_getCursorLocation(cursor);
  CXFile cursorFile;

  clang_getExpansionLocation(location, &cursorFile, nullptr, nullptr, nullptr);
  return filesystem::path(cxStringToStdString(clang_File_tryGetRealPathName(cursorFile)));
}

bool TwiliParser::is_included(const std::filesystem::path& path) const
{
  for (const std::string& directory : directories)
  {
    if (path.string().find(directory) == 0)
      return true;
  }
  return false;
}

std::string TwiliParser::get_relative_path() const
{
  std::string path = get_current_path().string();

  for (const std::string& directory : directories)
  {
    if (path.find(directory) != std::string::npos)
      return path.substr(directory.length());
  }
  return path;
}

bool TwiliParser::has_class(const std::string& class_name) const
{
  return std::find(classes.begin(), classes.end(), class_name) != classes.end();
}

std::vector<ClassDefinition> TwiliParser::get_classes() const
{
  std::vector<ClassDefinition> result;

  for (const auto& entry : classes) result.push_back(entry.klass);
  return result;
}

std::vector<NamespaceDefinition> TwiliParser::get_namespaces() const
{
  std::vector<NamespaceDefinition> result;

  for (const auto& entry : namespaces) result.push_back(entry.ns);
  return result;
}

std::vector<EnumDefinition> TwiliParser::get_enums() const
{
  vector<EnumDefinition> result;

  copy(enums.begin(), enums.end(), back_inserter(result));
  return result;
}

optional<string> TwiliParser::fullname_for(CXCursor cursor) const
{
  auto ns_it = std::find(namespaces.begin(), namespaces.end(), cursor);
  auto class_it = std::find(classes.begin(), classes.end(), cursor);

  if (ns_it != namespaces.end())
    return ns_it->ns.full_name;
  else if (class_it != classes.end())
    return class_it->klass.full_name;
  return optional<string>();
}

TwiliParser::ClassContext* TwiliParser::find_class_for(CXCursor cursor)
{
  auto it = std::find(classes.begin(), classes.end(), cursor);

  return it != classes.end() ? &(*it) : nullptr;
}

TwiliParser::ClassContext* TwiliParser::find_class_by_name(const std::string& full_name)
{
  auto it = std::find(classes.begin(), classes.end(), full_name);

  return it != classes.end() ? &(*it) : nullptr;
}

TwiliParser::ClassContext* TwiliParser::find_class_like(const std::string& symbol_name, const std::string& cpp_context)
{
  auto match = std::find(classes.begin(), classes.end(), cpp_context + "::" + symbol_name);

  if (match == classes.end()) // not an exact match
  {
    auto parts = Crails::split(cpp_context, ':');

    do
    {
      string parent_context;
      parts.remove(*parts.rbegin());
      for (const auto& part : parts) parent_context += "::" + part;
      match = std::find(classes.begin(), classes.end(), parent_context + "::" + symbol_name);
    }
    while (match == classes.end() && parts.size() > 0);
  }
  return match != classes.end() ? &(*match) : nullptr;
}

bool TwiliParser::operator()(CXTranslationUnit& unit)
{
  clang_visitChildren(
    clang_getTranslationUnitCursor(unit),
    &TwiliParser::visitor_callback,
    nullptr
  );
  return !find_parsing_errors(unit);
}

void TwiliParser::register_type(const ClassContext& new_class)
{
  TypeDefinition type_definition;

  type_definition.name = new_class.klass.name;
  type_definition.scopes = Crails::split<std::string, std::vector<std::string>>(new_class.klass.cpp_context(), ':');
  type_definition.type_full_name = new_class.klass.full_name;
  type_definition.kind = new_class.klass.type == "struct" ? StructKind : ClassKind;
  types.push_back(type_definition);
  classes.push_back(new_class);
  function_template_context = nullptr;
}

static bool are_types_identical(const TypeDefinition& a, const TypeDefinition& b)
{
  if (a.scopes.size() == b.scopes.size())
  {
    for (int i = 0 ; i < a.scopes.size() ; ++i)
    {
      if (a.scopes[i] != b.scopes[i])
        return false;
    }
  }
  return a.raw_name == b.raw_name &&
         a.name == b.name &&
         a.type_full_name == b.type_full_name;
}

CXChildVisitResult TwiliParser::visit_typedef(const std::string& symbol_name, CXCursor parent)
{
  auto cpp_context = fullname_for(parent);

  if (cpp_context)
  {
    CXType typedefType = clang_getCursorType(cursor);
    CXType type = clang_getTypedefDeclUnderlyingType(cursor);
    TypeDefinition pointed_from;
    TypeDefinition pointed_to;
    TypeDefinition explicit_from;

    pointed_from.load_from(type, types);
    pointed_to.load_from(typedefType, types);
    pointed_to.kind = TypedefKind;
    explicit_from.name = pointed_from.name;
    for (const auto& part : Crails::split(*cpp_context, ':'))
      explicit_from.scopes.push_back(part);
    for (const auto& part : pointed_from.scopes)
      explicit_from.scopes.push_back(part);

    optional<TypeDefinition> parent_type = explicit_from.find_parent_type(types);

    if (!parent_type)
      parent_type = pointed_from.find_parent_type(types);
    if (parent_type)
    {
      pointed_to.type_full_name = parent_type->type_full_name;
      pointed_to.is_const = pointed_to.is_const || parent_type->is_const;
      pointed_to.is_pointer += parent_type->is_pointer;
      pointed_to.is_reference += parent_type->is_reference;
    }
    else
    {
      TypeDefinition definite_type;
      definite_type.load_from(cxStringToStdString(clang_getTypeSpelling(type)), types);
      pointed_to.type_full_name = Crails::join(definite_type.scopes, "::") + "::" + definite_type.name;
    }
    pointed_to.declaration_scope = explicit_from.scopes;
    pointed_to.is_const = pointed_to.is_const || pointed_from.is_const;
    pointed_to.is_pointer += pointed_from.is_pointer;
    pointed_to.is_reference += pointed_from.is_reference;
    if (find_if(types.begin(), types.end(), bind(are_types_identical, pointed_to, placeholders::_1)) == types.end())
      types.push_back(pointed_to);
  }
  else
    cerr << "(i) Could not solve typedef " << symbol_name << endl;
  return CXChildVisit_Continue;
}

CXChildVisitResult TwiliParser::visit_namespace(const std::string& symbol_name, CXCursor parent)
{
  auto base_name = fullname_for(parent);
  auto full_name = (base_name ? *base_name : string()) + "::" + symbol_name;
  auto it = std::find(namespaces.begin(), namespaces.end(), full_name);

  if (it == namespaces.end())
  {
    NamespaceContext ns_context;

    ns_context.cursors.push_back(cursor);
    ns_context.ns.name = symbol_name;
    ns_context.ns.full_name = full_name;
    namespaces.push_back(ns_context);
  }
  else
    it->cursors.push_back(cursor);
  return CXChildVisit_Recurse;
}

CXChildVisitResult TwiliParser::visit_class(const std::string& symbol_name, CXCursor parent)
{
  auto kind = clang_getCursorKind(cursor);
  ClassContext new_class;
  ClassContext* parent_class;
  ClassContext* existing_class;

  new_class.klass.name = symbol_name;
  new_class.klass.from_file = get_current_path().string();
  new_class.klass.include_path = get_relative_path();
  new_class.cursors.push_back(cursor);
  new_class.current_access = kind == CXCursor_StructDecl ? CX_CXXPublic : CX_CXXPrivate;
  new_class.klass.type = kind == CXCursor_StructDecl ? "struct" : "class";
  if (parent.kind == CXCursor_TranslationUnit)
    new_class.klass.full_name = "::" + symbol_name;
  else if ((parent_class = find_class_for(parent)))
  {
    if (parent_class->current_access != CX_CXXPublic)
      return CXChildVisit_Continue;
    new_class.klass.full_name = parent_class->klass.full_name + "::" + symbol_name;
  }
  else
  {
    auto context_fullname = fullname_for(parent);

    if (context_fullname)
      new_class.klass.full_name = *context_fullname + "::" + symbol_name;
    else
    {
      TWILOG("(!) Couldn't find context for class " << symbol_name);
      return CXChildVisit_Continue;
    }
  }
  existing_class = find_class_by_name(new_class.klass.full_name);
  if (existing_class != nullptr)
  {
    if (existing_class->klass.is_empty())
    {
      existing_class->klass.from_file = get_current_path().string();
      existing_class->klass.include_path = get_relative_path();
    }
    existing_class->cursors.push_back(cursor);
    return existing_class->klass.is_empty() ? CXChildVisit_Recurse : CXChildVisit_Continue;
  }
  register_type(new_class);
  return CXChildVisit_Recurse;
}

static string remove_template_parameters(string source)
{
  size_t template_parameters_at = source.find("<");

  if (template_parameters_at != string::npos)
    source = source.substr(0, template_parameters_at);
  return source;
}

static string strip_declaration_type_from_class_declaration(string source)
{
  static const string class_declaration("class ");
  static const string struct_declaration("struct ");

  if (source.find(class_declaration) == 0)
    source = source.substr(class_declaration.size());
  else if (source.find(struct_declaration) == 0)
    source = source.substr(struct_declaration.size());
  return Crails::strip(source);
}

void TwiliParser::visit_base_class(ClassContext& current_class, const std::string& cursor_text)
{
  string symbol_name = strip_declaration_type_from_class_declaration(
    remove_template_parameters(cursor_text)
  );
  ClassContext* base_class = find_class_like(symbol_name, current_class.klass.full_name);

  if (base_class)
  {
    current_class.klass.bases.push_back(base_class->klass.full_name);
    current_class.klass.known_bases.push_back(base_class->klass.full_name);
  }
  else
  {
    current_class.klass.bases.push_back(symbol_name);
    TWILOG("(i) " << current_class.klass.full_name << " base class " << symbol_name << " cannot be solved");
  }
}

template<typename MODEL>
static void set_visibility_on(MODEL& model, CX_CXXAccessSpecifier access)
{
  switch (access)
  {
    default:
      model.visibility = "public";
      break ;
    case CX_CXXProtected:
      model.visibility = "protected";
      break ;
    case CX_CXXPrivate:
      model.visibility = "private";
      break ;
  }
}

CXChildVisitResult TwiliParser::visit_field(ClassContext& current_class, const string& symbol_name, bool is_static)
{
  FieldDefinition field(cursor, types);
  auto it = std::find(current_class.klass.fields.begin(), current_class.klass.fields.end(), field);

  if (it == current_class.klass.fields.end())
  {
    field.is_static = is_static;
    set_visibility_on(field, current_class.current_access);
    current_class.klass.fields.push_back(field);
  }
  return CXChildVisit_Continue;
}

MethodDefinition TwiliParser::create_method(const std::string& symbol_name, CXCursor)
{
  MethodDefinition new_method;
  CXType method_type = clang_getCursorType(cursor);
  CXType return_type = clang_getResultType(method_type);
  CXType arg_type;

  new_method.name = symbol_name;
  new_method.is_static = clang_CXXMethod_isStatic(cursor);
  new_method.is_virtual = clang_CXXMethod_isVirtual(cursor);
  new_method.is_pure_virtual = clang_CXXMethod_isPureVirtual(cursor);
  new_method.is_const = clang_CXXMethod_isConst(cursor);
  new_method.is_variadic = clang_Cursor_isVariadic(cursor);
  if (return_type.kind != 0 && return_type.kind != CXType_Void)
    new_method.return_type = ParamDefinition(return_type, types);
  for (int i = 0 ; (arg_type = clang_getArgType(method_type, i)).kind != 0 ; ++i)
  {
    CXCursor arg_cursor = clang_Cursor_getArgument(cursor, i);

    if (arg_cursor.kind != 0 && clang_getCursorType(arg_cursor).kind != 0)
      new_method.params.push_back(ParamDefinition(arg_cursor, types));
    else
      new_method.params.push_back(ParamDefinition(arg_type, types));
  }
  /*
  cout << "  -> with method `" << new_method.name << "`\n";
  if (new_method.return_type)
    cout << "    -> with return type " << new_method.return_type->to_string() << endl;
  for (const auto& param : new_method.params)
    cout << "    -> with param " << param.to_string() << endl;
  */
  return new_method;
}

CXChildVisitResult TwiliParser::visit_method(ClassContext& current_class, const string& symbol_name, CXCursor parent)
{
  auto kind = clang_getCursorKind(cursor);
  auto method = create_method(symbol_name, parent);
  auto& list = kind == CXCursor_Constructor
    ? current_class.klass.constructors
    : current_class.klass.methods;

  set_visibility_on(method, current_class.current_access);
  list.push_back(method);
  function_template_context = &(*list.rbegin());
  return CXChildVisit_Recurse;
}

FunctionDefinition TwiliParser::visit_function(const std::string& symbol_name, CXCursor parent)
{
  FunctionDefinition new_func;
  CXType method_type = clang_getCursorType(cursor);
  CXType return_type = clang_getResultType(method_type);
  CXType arg_type;
  auto context_name = fullname_for(parent);

  new_func.name = symbol_name;
  new_func.is_variadic = clang_Cursor_isVariadic(cursor);
  new_func.from_file = get_current_path().string();
  new_func.include_path = get_relative_path();
  if (context_name)
    new_func.full_name = *context_name + "::" + new_func.name;
  else
    new_func.full_name = "::" + new_func.name;
  if (return_type.kind != 0 && return_type.kind != CXType_Void)
    new_func.return_type = ParamDefinition(return_type, types);
  for (int i = 0 ; (arg_type = clang_getArgType(method_type, i)).kind != 0 ; ++i)
    new_func.params.push_back(ParamDefinition(arg_type, types));
  return new_func;
}

CXChildVisitResult TwiliParser::visit_template_parameter(ClassContext& class_context, const string& symbol_name)
{
  class_template_context = &class_context;
  class_context.klass.template_parameters.push_back({
    "typename",
    symbol_name
  });
  return CXChildVisit_Continue;
}

CXChildVisitResult TwiliParser::visit_enum(const string& symbol_name, CXCursor parent)
{
  auto cpp_context = fullname_for(parent).value_or("");
  auto existing_enum = std::find(enums.begin(), enums.end(), cpp_context + "::" + symbol_name);

  if (existing_enum == enums.end())
  {
    EnumContext new_context;

    new_context.en.name = symbol_name;
    new_context.en.full_name = cpp_context + "::" + symbol_name;
    new_context.en.from_file = get_current_path().string();
    new_context.cursor = cursor;

    TypeDefinition type_definition;
    type_definition.kind = EnumKind;
    type_definition.name = new_context.en.name;
    type_definition.scopes = Crails::split<std::string, std::vector<std::string>>(cpp_context, ':');
    type_definition.type_full_name = new_context.en.full_name;
    types.push_back(type_definition);
    enums.push_back(new_context);
  }
  return CXChildVisit_Recurse;
}

CXChildVisitResult TwiliParser::visit_enum_constant(const string& symbol_name, CXCursor parent)
{
  auto parent_enum = std::find(enums.begin(), enums.end(), parent);

  if (parent_enum != enums.end())
  {
    parent_enum->en.flags.push_back({symbol_name, clang_getEnumConstantDeclValue(cursor)});
  }
  return CXChildVisit_Recurse;
}

std::string TwiliParser::solve_typeref(CXCursor context)
{
  TypeDefinition type;
  auto declaration_scope = fullname_for(context);

  if (declaration_scope)
    type.declaration_scope = Crails::split<string, vector<string>>(*declaration_scope, ':');
  type.load_from(clang_getCursorType(cursor), types);
  return type.solve_type(types);
}

CXChildVisitResult TwiliParser::visit_template_default_value(const string& symbol_name, CXCursor parent)
{
  TemplateParameter& param = *class_template_context->klass.template_parameters.rbegin();
  string value = solve_typeref(parent);

  if (value != ("::" + param.name))
    param.default_value = solve_typeref(parent);
  return CXChildVisit_Continue;
}

optional<CXChildVisitResult> TwiliParser::try_to_visit_template_parameter(const string& symbol_name, CXCursor parent)
{
  auto kind = clang_getCursorKind(cursor);

  switch (kind)
  {
    case CXCursor_TemplateTypeParameter:
      function_template_context->template_parameters.push_back({"typename", symbol_name});
      return CXChildVisit_Continue ;
    case CXCursor_TypeRef:
      if (function_template_context->template_parameters.size())
      {
        auto& param = *function_template_context->template_parameters.rbegin();
        if (!param.default_value.length())
        {
          string value = solve_typeref(parent);

          if (value != ("::" + param.name))
            param.default_value = solve_typeref(parent);
          return CXChildVisit_Continue ;
        }
      }
      function_template_context = nullptr;
      return CXChildVisit_Continue ;
    case CXCursor_NamespaceRef:
      return CXChildVisit_Continue ;
    default:
      function_template_context = nullptr;
      break ;
  }
  return {};
}

CXChildVisitResult TwiliParser::visitor(CXCursor parent, CXClientData)
{
  print_state();
  if (is_included(get_current_path()))
  {
    auto kind = clang_getCursorKind(cursor);
    string symbol_name = cxStringToStdString(clang_getCursorSpelling(cursor));

    //cout << "Decl: " << clang_getCursorKindSpelling(clang_getCursorKind(cursor)) << " -> " <<  symbol_name << endl;
    if (class_template_context)
    {
      if (kind == CXCursor_TypeRef)
        visit_template_default_value(symbol_name, parent);
      class_template_context = nullptr;
    }
    if (function_template_context)
    {
      auto result = try_to_visit_template_parameter(symbol_name, parent);
      if (result) return *result;
    }
    if (kind == CXCursor_Namespace)
      return visit_namespace(symbol_name, parent);
    else if (kind == CXCursor_TypedefDecl)
      return visit_typedef(symbol_name, parent);
    else if (kind == CXCursor_EnumDecl)
      return visit_enum(symbol_name, parent);
    else if (kind == CXCursor_EnumConstantDecl)
      return visit_enum_constant(symbol_name, parent);
    else if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl || kind == CXCursor_ClassTemplate)
      return visit_class(symbol_name, parent);
    else
    {
      ClassContext* current_class = find_class_for(parent);

      if (current_class)
      {
        switch (kind)
        {
        case CXCursor_TemplateTypeParameter:
          visit_template_parameter(*current_class, symbol_name);
          break ;
        case CXCursor_CXXBaseSpecifier:
          visit_base_class(*current_class, symbol_name);
          break ;
        case CXCursor_CXXAccessSpecifier:
          current_class->current_access = clang_getCXXAccessSpecifier(cursor);
          break ;
        case CXCursor_FunctionTemplate:
        case CXCursor_CXXMethod:
          return visit_method(*current_class, symbol_name, parent);
        case CXCursor_FieldDecl:
          return visit_field(*current_class, symbol_name, false);
        case CXCursor_VarDecl:
          return visit_field(*current_class, symbol_name, true);
        case CXCursor_Constructor:
          return visit_method(*current_class, symbol_name, parent);
        default:
          //TWILOG("  Unhandled decl: " << clang_getCursorKindSpelling(clang_getCursorKind(cursor)) << " -> " <<  symbol_name);
          break ;
        }
        return CXChildVisit_Recurse;
      }
      else if (kind == CXCursor_FunctionDecl || kind == CXCursor_FunctionTemplate)
      {
        functions.push_back(visit_function(symbol_name, parent));
        if (kind == CXCursor_FunctionTemplate)
          function_template_context = &(*functions.rbegin());
        return CXChildVisit_Continue;
      }
      else
        TWILOG("Unhandled decl: " << clang_getCursorKindSpelling(clang_getCursorKind(cursor)) << " -> " <<  symbol_name);
    }
  }
  return CXChildVisit_Continue;
}

CXChildVisitResult TwiliParser::visitor_callback(CXCursor c, CXCursor parent, CXClientData clientData)
{
  parser->cursor = c;
  return parser->visitor(parent, clientData);
}
