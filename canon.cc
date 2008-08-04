#include "canon.hh"
#include "test.hh"

#include <vector>
#include <cstring>

std::string
canonicalize(char const* __restrict__ path_in)
{
  if (!*path_in)
    return "";

  char path[strlen(path_in)];
  std::strcpy(path, path_in);

  typedef std::vector<char const*> strv;
  strv components;

  bool initial_slash = false;
  char * it = path;
  while (char * lit = strsep(&it, "/"))
    {
      if (lit == path && *lit == 0)
	initial_slash = true;
      else if (it > lit + 1 || lit == path
	       || (it == NULL && *lit != 0))
	components.push_back(lit);
    }

  for (strv::iterator it = components.begin(); it != components.end(); )
    {
      if (std::strcmp(*it, ".") == 0)
	{
	  components.erase(it);
	  continue;
	}
      else if (std::strcmp(*it, "..") == 0
	       && it != components.begin()
	       && std::strcmp(*(it - 1), "..") != 0)
	{
	  components.erase(it - 1, it + 1);
	  if (it != components.begin())
	    --it;
	  continue;
	}
      ++it;
    }

  std::string ret = initial_slash ? "/" : "";
  for (strv::const_iterator it = components.begin();
       it != components.end(); ++it)
    if (**it)
      {
	if (it != components.begin())
	  ret += '/';
	ret += *it;
      }

  return ret;
}

#if defined SELFTEST
int
main(void)
{
  struct test {
    char const* path;
    char const* canon;
  };
  test t[] = {
    {"",""},
    {".",""},
    {"..",".."},
    {"/","/"},
    {"/.","/"},
    {"/..","/.."},
    {"/.././","/.."},
    {"/its/a/long/path/to/tipperary","/its/a/long/path/to/tipperary"},
    {"/simplify/this/path/../../if/possible","/simplify/if/possible"},
    {"simplify/this/path/../../if/possible","simplify/if/possible"},
    {"/simplify/this/path/../../if/possible/","/simplify/if/possible"},
    {"simplify/this/path/../../if/possible/","simplify/if/possible"},
    {"/a/b/c/../../../../d","/../d"},
    {"a/b/c/../../../../d","../d"},
    {"/a/b/c/../../../../d","/../d"},
    {"/a/b/c/../.././../../d","/../d"},
    {"a///////b/////////c////////d","a/b/c/d"},
    {"/////a///////b/////////c////////d","/a/b/c/d"},
    {"a///////b/////////c////..////d","a/b/d"},
    {"a///////b/////////c////.////d","a/b/c/d"},
    {"a/b/c/d/../../../../",""},
    {"/a/b/c/d/../../../../","/"},
    {"a/b/c/d/../../../../",""},
    {"a/b/c/d/../../e/../../","a"},
    {"a/b/c/d/../../../../",""},
    {"a/b/./c/d/","a/b/c/d"},
    {"/a/b/./c/d/","/a/b/c/d"},
    {"/.a/b/./c/d/","/.a/b/c/d"},
    {"a/b./c/d/","a/b./c/d"},
  };
  for (size_t i = 0; i < sizeof(t) / sizeof(*t); ++i)
    check(strcmp(canonicalize(t[i].path).c_str(), t[i].canon) == 0, t[i].path);
  end_tests();
}
#endif
