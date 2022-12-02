#pragma once
#include "definitions.hpp"
#include <filesystem>
#include <optional>
#include <algorithm>

class TwiliParser
{
  struct ClassContext
  {
    ClassDefinition       klass;
    CX_CXXAccessSpecifier current_access;
    std::vector<CXCursor> cursors;
    bool operator==(const std::string& value) const { return klass.full_name == value; }
    bool operator==(const CXCursor value) const
    {
      auto it = std::find_if(cursors.begin(), cursors.end(), [&value](const CXCursor candidate)
      {
        return clang_equalCursors(value, candidate);
      });
      return it != cursors.end();
    }
  };

  struct NamespaceContext
  {
    NamespaceDefinition   ns;
    std::vector<CXCursor> cursors;
    ClassContext          current_class;
    bool operator==(const std::string& value) const { return ns.full_name == value; }
    bool operator==(const CXCursor value) const
    {
      auto it = std::find_if(cursors.begin(), cursors.end(), [&value](const CXCursor candidate)
      {
        return clang_equalCursors(value, candidate);
      });
      return it != cursors.end();
    }
  };

  struct EnumContext
  {
    EnumDefinition en;
    CXCursor cursor;
    bool operator==(const std::string& value) const { return en.full_name == value; }
    bool operator==(const CXCursor value) const { return clang_equalCursors(value, cursor); }
    operator EnumDefinition() const { return en; }
  };

  std::vector<std::string>        directories;
  std::vector<TypeDefinition>     types;
  std::vector<ClassContext>       classes;
  std::vector<NamespaceContext>   namespaces;
  std::vector<FunctionDefinition> functions;
  std::vector<EnumContext>        enums;
  NamespaceContext                current_ns;
  NamespaceDefinition             root_ns;
  CXCursor                        cursor;
  ClassContext*                   class_template_context = nullptr;
  InvokableDefinition*            function_template_context = nullptr;
public:
  TwiliParser();
  TwiliParser(const TwiliParser&) = delete;
  ~TwiliParser();

  void add_directory(const std::string& path);
  void add_directory(const std::filesystem::path& path);
  const std::vector<std::string>& get_directories() const { return directories; }
  std::vector<ClassDefinition> get_classes() const;
  std::vector<NamespaceDefinition> get_namespaces() const;
  const std::vector<FunctionDefinition>& get_functions() const { return functions; }
  const std::vector<TypeDefinition>& get_types() const { return types; }
  std::vector<EnumDefinition> get_enums() const;

  std::filesystem::path get_current_path() const;
  std::string           get_relative_path() const;
  bool                  is_included(const std::filesystem::path& path) const;
  bool                  has_class(const std::string& class_name) const;
  bool                  operator()(CXTranslationUnit& unit);

private:
  static CXChildVisitResult visitor_callback(CXCursor c, CXCursor parent, CXClientData clientData);

  CXChildVisitResult visitor(CXCursor parent, CXClientData clientData);
  CXChildVisitResult visit_class(const std::string& symbol_name, CXCursor parent);
  CXChildVisitResult visit_template_parameter(ClassContext& class_context, const std::string& symbol_name);
  CXChildVisitResult visit_template_default_value(const std::string& symbol_name, CXCursor parent);
  CXChildVisitResult visit_method(ClassContext& class_context, const std::string& symbol_name, CXCursor parent);
  void               visit_base_class(ClassContext& class_context, const std::string& symbol_name);
  CXChildVisitResult visit_namespace(const std::string& symbol_name, CXCursor parent);
  CXChildVisitResult visit_typedef(const std::string& symbol_name, CXCursor parent);
  FunctionDefinition visit_function(const std::string& symbol_name, CXCursor parent);
  CXChildVisitResult visit_field(ClassContext&, const std::string& symbol_name, bool is_static);
  CXChildVisitResult visit_enum(const std::string& symbol_name, CXCursor parent);
  CXChildVisitResult visit_enum_constant(const std::string& symbol_name, CXCursor parent);
  std::optional<CXChildVisitResult> try_to_visit_template_parameter(const std::string& symbol_name, CXCursor parent);

  std::optional<std::string> fullname_for(CXCursor) const;
  ClassContext* find_class_for(CXCursor);
  ClassContext* find_class_by_name(const std::string& full_name);
  ClassContext* find_class_like(const std::string& symbol_name, const std::string& cpp_context);

  MethodDefinition create_method(const std::string& symbol_name, CXCursor parent);
  void register_type(const ClassContext&);
  std::string solve_typeref(CXCursor context);
  void print_state();
};
