#ifndef cgt_cgfile_hh_guard
#define cgt_cgfile_hh_guard

#include "symbol.ii"
#include "types.hh"
#include "reader.hh"
#include "quark.hh"

class cgfile {
  typedef std::MAP<unsigned, ProgramSymbol*> id_psym_map;
  typedef std::MAP<q::Quark, ProgramSymbol*> name_psym_map;
  typedef std::MAP<q::Quark, FileSymbol*> name_fsym_map;

public:
  // Public read-only view of internal data...
  psym_vect const& all_program_symbols;
  name_fsym_map const& file_symbols;
  name_psym_map const& global_symbols;

  cgfile();
  ~cgfile();

  void include(tok_vect_vect const& file_tokens, char const* curmodule);
  void include(char const* filename);
  void sort_psyms_by_file();

  // `include' doesn't compute callers by default, only callees.  Call
  // this function to have callers re/computed.
  void compute_callers();

  // `include' doesn't compute used symbols, everything is "unused" by
  // default.
  void compute_used();

  psym_vect const& get_symbols() const { return m_all_program_symbols; }

private:
  void clean();

  psym_vect m_all_program_symbols;
  name_fsym_map m_file_symbols;

  /// Maps names of global symbols to their declarations and
  /// definitions.  Most symbols will start their life as decls, and
  /// when suitable definition is encountered, the global symbol is
  /// adjusted to become a definition.
  name_psym_map m_global_symbols;

  // - v - - - - - - - - - - - - - - - - - - - - - - - - - - - - - v -

  // The variables that follow conceptually belong to `include'
  // routine.  They are instance variables to speed processing up:
  // memory for containers doesn't have to be free'd/malloc'd/resized
  // each time new file is included.  Significantly improves linker
  // performance.

  // Assignments [symbol ID -> symbol] and [symbol name -> symbol]
  // that were seen in the file being processed.
  id_psym_map m_id_assignments;
  name_psym_map m_name_assignments;

  // Map of alias symbols waiting for canonical symbol to show up.
  typedef std::vector<std::pair<q::Quark, ProgramSymbol*> > pending_aliases_map;
  pending_aliases_map m_pending_aliases;

  // Map of callee IDs waiting for their symbol to show up.
  // Mapping ID -> callers.
  typedef std::vector<std::pair<unsigned, ProgramSymbol*> > pending_callees_t;
  pending_callees_t m_pending_callees;

  // - ^ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ^ -

  friend class cgfile_binder;
};

#endif//cgt_cgfile_hh_guard
