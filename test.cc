#include "test.hh"
#include <iostream>
#include <cstdlib>

namespace {
  bool errors = false;
}

void
check(bool result, char const* test)
{
  if (!result)
    {
      std::cout << "FAIL: `" << test << "'" << std::endl;
      ::errors = true;
    }
}

void
end_tests()
{
  if (!::errors)
    std::cout << "all passed" << std::endl;
  std::exit((int)::errors);
}
