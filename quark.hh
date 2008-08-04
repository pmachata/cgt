#include <string>

namespace q {
  struct QuarkS;
  typedef QuarkS const* Quark;

  Quark intern(std::string const& str);
  std::string const* to_string(Quark q);
  unsigned long to_ulong(Quark q);
}
