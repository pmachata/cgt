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

ProgramSymbol::ProgramSymbol(std::string name, FileSymbol *file, unsigned line)
  : Symbol(name)
  , m_id(gen_id())
  , m_file(file)
  , m_line_number(line)
  , m_is_static(false)
  , m_is_decl(false)
  , m_is_var(false)
  , m_used(false)
  , m_forward_to(NULL)
  , m_callers(NULL)
{
}

ProgramSymbol::~ProgramSymbol()
{
  clear_callers();
}

void
ProgramSymbol::add_callee(ProgramSymbol * sym)
{
  m_callees.insert(sym);
}

void
ProgramSymbol::clear_callers()
{
  delete m_callers;
}

void
ProgramSymbol::check_have_callers() const
{
  if (m_callers == NULL)
    m_callers = new psym_set();
}

void
ProgramSymbol::add_caller(ProgramSymbol * sym)
{
  check_have_callers();
  m_callers->insert(sym);
}

psym_set const&
ProgramSymbol::get_callers() const
{
  check_have_callers();
  return *m_callers;
}

void
ProgramSymbol::resolve_callee_aliases()
{
  psym_set nc;
  for (psym_set::iterator it = m_callees.begin();
       it != m_callees.end(); ++it)
    {
      ProgramSymbol *callee = *it;
      while (callee->m_forward_to != NULL)
	callee = callee->m_forward_to;
      nc.insert(callee);
    }
  m_callees = nc;
}

void
ProgramSymbol::set_forward_to(ProgramSymbol *other)
{
  assert (m_forward_to == NULL);
  m_forward_to = other;
}

void
ProgramSymbol::dump(std::ostream & o) const
{
  o << m_id << ' ' << '(' << m_line_number << ')'
    << (m_is_static ? " @static" : "")
    << (m_is_decl ? " @decl" : "")
    << (m_is_var ? " @var" : "")
    << ' ' << get_name();

  std::vector<ProgramSymbol *> v(m_callees.begin(), m_callees.end());
  std::sort(v.begin(), v.end(), cmp_id());
  for (auto const &psym: v)
    o << ' ' << psym->get_id();
  o << std::endl;
}

void
update_path(ProgramSymbol *psym, std::string const& curpath)
{
  static std::string HOME = std::getenv("HOME") ?: "";

  if (FileSymbol *fsym = psym->get_file())
    if (std::string const& fn = fsym->get_name();
        fn != "")
      {
        if (fn[0] == '/' || fn[0] == '<')
          // system headers and built-ins
          psym->set_path(fsym->get_name());
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
            psym->set_path(canonicalize(path));
          }
      }
}
