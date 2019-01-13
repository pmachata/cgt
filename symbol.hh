#ifndef cgt_symbol_hh_guard
#define cgt_symbol_hh_guard

#include "id.hh"
#include "quark.hh"
#include "symbol.ii"

#include <cassert>
#include <iosfwd>
#include <string>

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
  void check_have_callers() const;

public:
  ProgramSymbol(q::Quark name, FileSymbol *file, unsigned line);
  ~ProgramSymbol();

  unsigned get_id() const { return m_id; }

  void add_callee(ProgramSymbol * sym);
  psym_set const& get_callees() const { return m_callees; }
  void resolve_callee_aliases();

  void clear_callers();
  void add_caller(ProgramSymbol * sym);
  psym_set const& get_callers() const;

  void set_forward_to(ProgramSymbol *other);
  bool is_forwarder() const { return m_forward_to != NULL; }

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
  std::string const& get_path() const { return *q::to_string(m_path); }
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

void update_path(ProgramSymbol *psym, std::string const& curpath);

#endif//cgt_symbol_hh_guard
