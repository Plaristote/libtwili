#include "definitions.hpp"

using namespace std;

bool MethodDefinition::operator==(const MethodDefinition& other) const
{
  if (name == other.name && params.size() == other.params.size())
  {
    for (int i = 0 ; i < params.size() ; ++i)
    {
      if (params[i] != other.params[i])
        return false;
    }
    return true;
  }
  return false;
}

