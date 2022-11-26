#include "definitions.hpp"
#include <crails/utils/split.hpp>
#include <crails/utils/join.hpp>

using namespace std;

string FunctionDefinition::cpp_context() const
{
  auto parts = Crails::split(full_name, ':');
  auto last = parts.begin();

  advance(last, parts.size() - 1);
  parts.erase(last);
  return "::" + Crails::join(parts, "::");
}
