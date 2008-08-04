#include "canon.hh"
#include "symbol.hh"

#include <algorithm>
#include <cstring>
#include <iostream>

struct cmp_id {
  bool operator()(ProgramSymbol* a, ProgramSymbol* b) {
    return a->get_id() < b->get_id();
  }
};

void
ProgramSymbol::dump(std::ostream & o) const
{
  o << m_id << ' ' << '(' << m_line_number << ')'
    << (m_is_static ? " @static" : "")
    << (m_is_decl ? " @decl" : "")
    << (m_is_var ? " @var" : "")
    << ' ' << get_name();

  psym_vect v(m_callees.begin(), m_callees.end());
  std::sort(v.begin(), v.end(), cmp_id());
  for (psym_vect::const_iterator it = v.begin();
       it != v.end(); ++it)
    o << ' ' << (*it)->get_id();
  o << std::endl;
}

ProgramSymbol*
psym_ptrcall()
{
  static ProgramSymbol *const psym = new ProgramSymbol(q::intern("*"), NULL, 0);
  return psym;
}

void
update_path(ProgramSymbol *psym, std::string const& curpath)
{
  static std::string HOME = std::getenv("HOME") ?: "";

  FileSymbol *fsym = psym->get_file();
  if (fsym == NULL || fsym->get_qname() == NULL)
    {
    empty:
      psym->set_path(q::intern(""));
    }
  else
    {
      std::string const& fn = fsym->get_name();
      if (fn.length() == 0)
	goto empty;

      if (fn[0] == '/' || fn[0] == '<')
	// system headers and built-ins
	psym->set_path(fsym->get_qname());
      else
	{
	  char pathstor[curpath.length() + fn.length() + 1];
	  size_t pathlen = sizeof(pathstor);
	  char *path = pathstor;
	  stpcpy(stpcpy(path, curpath.c_str()), fn.c_str());
	  if (!HOME.empty() && pathlen > HOME.length()
	      && std::strncmp(path, HOME.c_str(), HOME.length()) == 0
	      && (*HOME.rbegin() == '/' || path[HOME.length()] == '/'))
	    {
	      path += HOME.length() - 1;
	      *path = '~';
	    }
	  psym->set_path(q::intern(canonicalize(path)));
	}
    }
}
