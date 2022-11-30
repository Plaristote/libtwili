#pragma once
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <clang-c/Index.h>

struct TemplateParameter
{
  std::string type;
  std::string name;
  std::string default_value;
};

typedef std::vector<TemplateParameter> TemplateParameters;

struct TypeDefinition
{
  std::string              raw_name;
  std::string              name;
  std::vector<std::string> scopes;
  std::string              type_full_name;
  bool                     is_const = false;
  int                      is_reference = 0;
  int                      is_pointer = 0;

  TypeDefinition& load_from(CXType, const std::vector<TypeDefinition>& known_types);
  TypeDefinition& load_from(const std::string& name, const std::vector<TypeDefinition>& known_types);
  unsigned char   type_match(const TypeDefinition&);
  std::string     solve_type(const std::vector<TypeDefinition>& known_types);
  std::optional<TypeDefinition> find_parent_type(const std::vector<TypeDefinition>& known_types);
  std::string     to_string() const;
  std::string     to_full_name() const;
};

struct EnumDefinition
{
  std::string name;
  std::string full_name;
  std::unordered_map<std::string, long long> flags;
};

struct ParamDefinition : public std::string
{
  ParamDefinition() {}
  ParamDefinition(CXCursor cursor, const std::vector<TypeDefinition>& known_types);
  ParamDefinition(CXType type, const std::vector<TypeDefinition>& known_types);
  ParamDefinition(const std::string& name) : std::string(name) {}

  bool        is_const     = false;
  int         is_reference = 0;
  int         is_pointer   = 0;
  std::string name;
  std::string type_alias;

  std::string to_string() const;

  bool operator==(const ParamDefinition& other) const { return to_string() == other.to_string(); }
private:
  void initialize_type(CXType type, const std::vector<TypeDefinition>& known_types);
};

struct FieldDefinition : public ParamDefinition
{
  FieldDefinition() {}
  FieldDefinition(CXCursor cursor, const std::vector<TypeDefinition>& known_types) : ParamDefinition(cursor, known_types) {}
  bool        is_static = false;
  std::string visibility;

  bool operator==(const FieldDefinition& other) const { return name == other.name; }
};

struct InvokableDefinition
{
  std::optional<ParamDefinition> return_type;
  std::vector<ParamDefinition>   params;
  TemplateParameters template_parameters;
  bool is_variadic = false;
  bool is_template() const { return template_parameters.size() > 0; }
};

struct MethodDefinition : public InvokableDefinition
{
  bool               is_static = false;
  bool               is_virtual = false;
  bool               is_pure_virtual = false;
  bool               is_const = false;
  std::string        name;
  std::string        visibility;

  bool operator==(const MethodDefinition&) const;
};

struct FunctionDefinition : public InvokableDefinition
{
  std::string name;
  std::string full_name;
  std::string from_file;
  std::string include_path;
  std::string cpp_context() const;
};

struct NamespaceDefinition
{
  std::string name;
  std::string full_name;
  std::string cpp_context() const;
  bool operator==(const std::string& value) const { return full_name == value; }
};

struct ClassDefinition : public NamespaceDefinition
{
  std::string                   type;
  std::string                   from_file;
  std::string                   include_path;
  std::vector<std::string>      bases;
  std::vector<std::string>      known_bases;
  std::vector<MethodDefinition> constructors;
  std::vector<MethodDefinition> methods;
  std::vector<FieldDefinition>  fields;
  TemplateParameters            template_parameters;
  bool is_empty() const { return constructors.size() + methods.size() + bases.size() == 0; }
  bool is_template() const { return template_parameters.size() > 0; }
  bool implements(const MethodDefinition&) const;
};
