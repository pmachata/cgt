#include "cgfile.hh"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <boost/lambda/lambda.hpp>

cgfile::cgfile()
  : all_program_symbols(m_all_program_symbols)
  , file_symbols(m_file_symbols)
  , global_symbols(m_global_symbols)
{
  m_all_program_symbols.push_back(psym_ptrcall());
}

cgfile::~cgfile()
{
  for (psym_vect::iterator it = m_all_program_symbols.begin();
       it != m_all_program_symbols.end(); ++it)
    delete *it;

  for (name_fsym_map::iterator it = m_file_symbols.begin();
       it != m_file_symbols.end(); ++it)
    delete it->second;
}

void
cgfile::clean()
{
  m_id_assignments.clear();
  m_name_assignments.clear();

  // Don't call `clear' here, STL implementation is allowed to release
  // the memory.  We don't want that, it decreases the performance.
  m_pending_aliases.resize(0);
  m_pending_callees.resize(0);
}

void
cgfile::include(char const* filename)
{
  tok_vect_vect file_tokens;
  fd_reader *rd = open_or_die(filename);
  tokenize_file(rd, file_tokens);
  include(file_tokens, filename);
  delete rd;
}

void
cgfile::include(tok_vect_vect const& file_tokens, char const* curmodule)
{
  clean();
  m_id_assignments[psym_ptrcall()->get_id()] = psym_ptrcall();

  FileSymbol *fsym = NULL;
  q::Quark filename = NULL;
  unsigned line_number = 0;

  std::string curpath(curmodule);
  size_t idx = curpath.find_last_of('/');
  if (idx != std::string::npos)
    curpath = curpath.substr(0, idx + 1);
  else
    curpath = "./";

  size_t num_lines = file_tokens.size();
  for (size_t line_i = 0; line_i < num_lines; ++line_i)
    {
      tok_vect const& tokens = file_tokens[line_i];
      tok_vect::size_type tokens_size = tokens.size();
      char const* strp = tokens[0];

      if (strp[0] == 'F' && strp[1] == 0)
	{
	  filename = q::intern(tokens[1]);
	  continue;
	}

      // line structure: <id> (<linedef>) (@decl|@var|@static)* <name> (<callee id>)*

      // <id> (<linedef>)
      unsigned long id = std::strtoul(tokens[0], NULL, 10);
      strp = tokens[1] + 1; // skip opening paren
      line_number = std::strtoul(strp, NULL, 10); // never mind closing paren

      bool is_decl = false;
      bool is_var = false;
      bool is_static = false;
      size_t i = 2;
      q::Quark name = NULL;
      while (true)
	{
	  strp = tokens[i++];
	  if (strp[0] != '@')
	    {
	      name = q::intern(strp);
	      break;
	    }

	  switch (strp[1]) {
	  case 'd':
	    if (strp[2] == 'e' && strp[3] == 'c'
		&& strp[4] == 'l' && strp[5] == 0)
	      is_decl = true;
	    break;
	  case 'v':
	    if (strp[2] == 'a' && strp[3] == 'r' && strp[4] == 0)
	      is_var = true;
	    break;
	  case 's':
	    if (strp[2] == 't' && strp[3] == 'a'
		&& strp[4] == 't' && strp[5] == 'i'
		&& strp[6] == 'c' && strp[7] == 0)
	      is_static = true;
	    break;
	  }
	}

      if (fsym == NULL || fsym->get_qname() != filename)
	{
	  name_fsym_map::iterator it = m_file_symbols.find(filename);
	  if (it != m_file_symbols.end())
	    fsym = it->second;
	  else
	    {
	      fsym = new FileSymbol(filename);
	      m_file_symbols[filename] = fsym;
	    }
	}

      id_psym_map::const_iterator lsit;
      name_psym_map::const_iterator gsit;
      bool maybe_enlist = false;
      ProgramSymbol *psym = NULL;
      q::Quark canon = NULL;

      // This is an alias.  It's like any other symbol decl, except it
      // aliases other symbol.
      if (i < tokens_size
	  && ((strp = tokens[i])[0] == '-'
	      && strp[1] == '>' && strp[2] == 0))
	{
	  canon = q::intern(tokens[i+1]);
	  i += 2; // skip arrow and canon name
	}

      // Look if there is an external symbol that we could bind this
      // one to (but skip this step if the symbol being considered is
      // static).  This covers the case where there is definition or
      // declaration somewhere out there, and we have found matching
      // declaration or definition.
      if (!is_static
	  && ((gsit = m_global_symbols.find(name))
	      != m_global_symbols.end()))
	psym = gsit->second;
      // Look if there is a local symbol that we could bind this one
      // to.  Covers the case where we're seeing another declaration
      // or definition of already declared/defined function (i.e. they
      // have the same ID).
      else if ((lsit = m_id_assignments.find(id)) != m_id_assignments.end())
	{
	  psym = lsit->second;
	  std::string const& nn = *q::to_string(name);
	  if (nn != psym->get_name())
	    std::cerr << "warning: " << curmodule
		      << ": symbol #" << id
		      << " renamed from `" << psym->get_name()
		      << "' to `" << nn << '\'' << std::endl;
	}

      // Above, we have bound a symbol.  Check the sanity of
      // that binding.
      if (psym != NULL)
	{
	  // Tolerate redefinition.  This can easily happen when
	  // "linking" graphs that actually form a family of
	  // projects.
	  // @TODO: warn if redefined symbols are callees
	  if (!psym->is_decl() && !is_decl
	      && psym->get_file() != fsym)
	    psym = NULL;
	  else
	    {
	      if (unlikely (psym->is_static() != is_static))
		std::cerr << "warning: " << curmodule << ": "
			  << (psym->is_static() ? "static" : "non-static")
			  << " symbol " << psym->get_name()
			  << " redefined as "
			  << (is_static ? "static" : "non-static")
			  << std::endl;
	      if (unlikely (psym->is_var() != is_var))
		std::cerr << "warning: " << curmodule << ": "
			  << (psym->is_var() ? "variable" : "function")
			  << " symbol " << psym->get_name() << " redefined as "
			  << (is_static ? "variable" : "function") << std::endl;
	    }

	  if (psym != NULL)
	    {
	      if (psym->is_decl()
		  && (!is_decl || (psym->get_line_number() == 0
				   && line_number != 0)))
		{
		  psym->set_file(fsym);
		  psym->set_line_number(line_number);
		  update_path(psym, curpath);
		}

	      if (!is_decl)
		{
		  psym->set_decl(false);
		  psym->set_line_number(line_number);
		  maybe_enlist = true;
		}
	    }
	}

      // We haven't found anything, or what we found is redefinition.
      // Create a new symbol.
      if (psym == NULL)
	{
	  psym = new ProgramSymbol(name, fsym, line_number);
	  m_all_program_symbols.push_back(psym);
	  psym->set_decl(is_decl);
	  psym->set_static(is_static);
	  psym->set_var(is_var);
	  update_path(psym, curpath);
	  maybe_enlist = true;
	}

      // Handle aliases.  If we've already seen this name.  Make
      // the new symbol alias the other one.  Otherwise make it
      // pending alias.  Only look up local symbols, because
      // aliases have to be resolved locally.
      if (canon != NULL)
	{
	  name_psym_map::const_iterator it
	    = m_name_assignments.find(canon);

	  if (it != m_name_assignments.end())
	    psym->set_forward_to(it->second);
	  else
	    m_pending_aliases.push_back(std::make_pair(canon, psym));
	}

      m_id_assignments[id] = psym;
      m_name_assignments[name] = psym;
      if (!is_decl)
	psym->set_used();

      // This is a function with body.  Look through the call list.
      if (!is_decl && !is_var)
	while (i < tokens_size)
	  {
	    strp = tokens[i++];
	    unsigned callee_id;
	    if (strp[0] == '*' && strp[1] == 0)
	      callee_id = psym_ptrcall()->get_id();
	    else
	      {
		callee_id = std::strtoul(strp, NULL, 10);
		if (unlikely (callee_id == 0))
		  std::cerr << "warning: " << curmodule
			    << ": symbol " << psym->get_name()
			    << " calls suspicious id " << strp << std::endl;
	      }

	    id_psym_map::const_iterator it;
	    // If we have already seen the declaration, resolve
	    // the callee right away.  Otherwise add it among
	    // pending callees.
	    if ((it = m_id_assignments.find(callee_id))
		!= m_id_assignments.end())
	      {
		ProgramSymbol *callee = it->second;
		psym->add_callee(callee);
	      }
	    else
	      m_pending_callees.push_back(std::make_pair(callee_id, psym));
	  }

      if (maybe_enlist && !is_static)
	m_global_symbols[name] = psym;
    }

  // Resolve pending aliases.
  for (pending_aliases_map::iterator it = m_pending_aliases.begin();
       it != m_pending_aliases.end(); ++it)
    {
      name_psym_map::const_iterator kt = m_name_assignments.find(it->first);
      if (likely (kt != m_name_assignments.end()))
	it->second->set_forward_to(kt->second);
      else
	std::cerr << "warning: " << curmodule
		  << ": a symbol " << it->second->get_name()
		  << " aliases unknown symbol named "
		  << *q::to_string(it->first) << std::endl;
    }

  // Resolve pending callees.
  for (pending_callees_t::iterator it = m_pending_callees.begin();
       it != m_pending_callees.end(); ++it)
    {
      id_psym_map::const_iterator kt = m_id_assignments.find(it->first);
      if (likely (kt != m_id_assignments.end()))
	it->second->add_callee(kt->second);
      else
	std::cerr << "warning: " << curmodule
		  << ": unresolved call from "
		  << it->second->get_name()
		  << " to symbol #" << it->first << std::endl;
    }

  // Redirect callees to their canonical, because the following is legal:
  //   - declare X
  //   - define Y
  //   - define Z which calls X
  //   - declare that X aliases Y
  // So we have to move the call graph arrows for Z from ->X to ->Y.
  for (id_psym_map::const_iterator it = m_id_assignments.begin();
       it != m_id_assignments.end(); ++it)
    it->second->resolve_callee_aliases();

  clean();
}

namespace {
  struct compare_psyms_file {
    bool operator()(ProgramSymbol *ps1, ProgramSymbol *ps2) {
      return ps1->get_file() < ps2->get_file();
    }
  };
}

void
cgfile::sort_psyms_by_file()
{
  std::sort(m_all_program_symbols.begin(),
	    m_all_program_symbols.end(),
	    ::compare_psyms_file());
}

void
cgfile::compute_callers()
{
  std::for_each(m_all_program_symbols.begin(),
		m_all_program_symbols.end(),
		boost::lambda::_1 ->* &ProgramSymbol::clear_callers);

  for (psym_vect::iterator it = m_all_program_symbols.begin();
       it != m_all_program_symbols.end(); ++it)
    {
      psym_set const& callees = (*it)->get_callees();
      for (psym_set::iterator jt = callees.begin();
	   jt != callees.end(); ++jt)
	(*jt)->add_caller(*it);
    }
}
