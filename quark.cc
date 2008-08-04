#include "quark.hh"
#include "types.hh"

#include <cassert>
#include <cstdlib>

namespace {
  typedef std::SET<std::string> interned_t;
  interned_t interned;
}

q::Quark
q::intern(std::string const& str)
{
  ::interned_t::const_iterator it = ::interned.find(str);
  if (it == ::interned.end())
    it = ::interned.insert(str).first;
  assert(it != ::interned.end());
  std::string const* ptr = &*it;
  return reinterpret_cast<q::Quark>(ptr);
}

std::string const*
q::to_string(q::Quark q)
{
  return reinterpret_cast<std::string const*>(q);
}

unsigned long
q::to_ulong(q::Quark q)
{
  char const* iddef = to_string(q)->c_str();
  return std::strtoul(iddef, NULL, 10);
}
