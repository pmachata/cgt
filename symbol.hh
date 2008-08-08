#ifndef cgt_symbol_hh_guard
#define cgt_symbol_hh_guard

#include "id.hh"
#include "quark.hh"
#include "types.hh"

#include <cassert>
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

struct FileSymbol;
struct ProgramSymbol;

typedef std::set<ProgramSymbol *> psym_set;
typedef std::SET<ProgramSymbol *> psym_SET;
typedef std::vector<ProgramSymbol *> psym_vect;

struct Symbol {
  Symbol(q::Quark name)
    : m_name(name)
  {}

  std::string const& get_name() const {
    return *q::to_string(m_name);
  }

  q::Quark get_qname() const {
    return m_name;
  }

private:
  q::Quark const m_name;
};

struct FileSymbol
  : public Symbol
{
  FileSymbol(q::Quark name) : Symbol(name) { }
};

class ProgramSymbol
  : public Symbol
{
  void check_have_callers() const {
    if (m_callers == NULL)
      m_callers = new psym_set();
  }

public:
  ProgramSymbol(q::Quark name, FileSymbol *file, unsigned line)
    : Symbol(name)
    , m_id(gen_id())
    , m_file(file)
    , m_path(NULL)
    , m_line_number(line)
    , m_is_static(false)
    , m_is_decl(false)
    , m_is_var(false)
    , m_used(false)
    , m_forward_to(NULL)
    , m_callers(NULL)
  {
  }

  ~ProgramSymbol() {
    clear_callers();
  }

  unsigned get_id() const {
    return m_id;
  }

  void add_callee(ProgramSymbol * sym) {
    m_callees.insert(sym);
  }

  psym_set const& get_callees() const {
    return m_callees;
  }

  void clear_callers() {
    delete m_callers;
  }

  void add_caller(ProgramSymbol * sym) {
    check_have_callers();
    m_callers->insert(sym);
  }

  psym_set const& get_callers() const {
    check_have_callers();
    return *m_callers;
  }

  void resolve_callee_aliases() {
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

  void set_forward_to(ProgramSymbol *other) {
    assert (m_forward_to == NULL);
    m_forward_to = other;
  }

  bool is_forwarder() const {
    return m_forward_to != NULL;
  }

  void set_static(bool is_static) { m_is_static = is_static; }
  void set_decl(bool is_decl) { m_is_decl = is_decl; }
  void set_var(bool is_var) { m_is_var = is_var; }

  bool is_static() const { return m_is_static; }
  bool is_decl() const { return m_is_decl; }
  bool is_var() const { return m_is_var; }

  void set_line_number(unsigned line_number) { m_line_number = line_number; }
  unsigned get_line_number() const { return m_line_number; }

  void set_used() { m_used = true; }
  bool is_used() const { return m_used; }

  void set_file(FileSymbol *file) { m_file = file; }
  FileSymbol *get_file() const { return m_file; }

  void set_path(q::Quark path) { m_path = path; }
  std::string const& get_path() const {
    return *q::to_string(m_path);
  }
  q::Quark get_qpath() const { return m_path; }

  void dump(std::ostream & o) const;

private:
  unsigned const m_id;
  FileSymbol *m_file;
  q::Quark m_path;
  unsigned m_line_number;
  bool m_is_static, m_is_decl, m_is_var;
  bool m_used; // whether anyone calls it
  ProgramSymbol *m_forward_to; // set if this symbol is an alias
  psym_set m_callees;
  mutable psym_set * m_callers;

  friend class ProgramSymbol_binder;
};

// Pseudo-symbol used as calee in "call through pointer" cases.
ProgramSymbol* psym_ptrcall();

void update_path(ProgramSymbol *psym, std::string const& curpath);

#endif//cgt_symbol_hh_guard
