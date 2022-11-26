#include "definitions.hpp"

bool ClassDefinition::implements(const MethodDefinition& method) const
{
  for (const auto& candidate : methods)
  {
    if (candidate == method)
      return true;
  }
  return false;
}
