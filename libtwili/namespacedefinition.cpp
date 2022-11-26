#include "definitions.hpp"
#include <crails/utils/split.hpp>
#include <sstream>

using namespace std;

string NamespaceDefinition::cpp_context() const
{
  auto parts = Crails::split(full_name, ':');
  auto last = parts.begin();
  stringstream stream;

  if (parts.size() > 1)
  {
    advance(last, parts.size() - 1);
    parts.erase(last);
    for (const auto& part : parts)
      stream << "::" << part;
  }
  else
    stream << "::";
  return stream.str();
}
