#include "cgfile.hh"
#include "symbol.hh"

#include <boost/python.hpp>
#include <iostream>

using namespace boost::python;

template<class B>
struct my_iterator {
  virtual B next() = 0;
};

class psym_it {
  typedef my_iterator<ProgramSymbol *> psym_iterator;
  psym_iterator * it;
public:
  psym_it(psym_iterator * it) : it(it) {}
  ProgramSymbol &next() {
    if (ProgramSymbol *sym = it->next())
      return *sym;
    else
      {
	delete it;
	PyErr_SetNone(PyExc_StopIteration);
	throw_error_already_set();
	// notreached
	return *static_cast<ProgramSymbol*>(NULL);
      }
  }
};

template<class C>
struct cont_iterator
  : public my_iterator<typename C::value_type>
{
  typename C::iterator it, et;

  cont_iterator(C & cont)
    : it(cont.begin()), et(cont.end())
  {}

  virtual typename C::value_type next() {
    if (it == et)
      return NULL;
    else
      return *it++;
  }
};

template<class C>
psym_it *psym_iter(C & self) {
  return new psym_it(new cont_iterator<C>(self));
}

struct cgfile_binder {
  static psym_vect &all_program_symbols(cgfile & self) {
    return self.m_all_program_symbols;
  }
};


struct ProgramSymbol_binder {
  static char const* psym_get_name(ProgramSymbol & self) {
    return self.get_name().c_str();
  }

  static char const* fsym_get_name(FileSymbol & self) {
    return self.get_name().c_str();
  }

  static char const* get_file_name(ProgramSymbol & self) {
    if (self.get_file() == NULL)
      return "";
    return self.get_file()->get_name().c_str();
  }

  static int hash(ProgramSymbol & self) {
    return static_cast<int>(self.get_id());
  }

  static int cmp(ProgramSymbol & self, ProgramSymbol & other) {
    unsigned q1 = self.get_id();
    unsigned q2 = other.get_id();
    if (q1 < q2)
      return -1;
    else if (q1 > q2)
      return 1;
    else
      return 0;
  }

  static char const* repr(ProgramSymbol & self) {
    return self.get_name().c_str();
  }
};


BOOST_PYTHON_MODULE(cgt)
{
  class_<psym_it>("psym_it", no_init)
    .def("next", &psym_it::next,
	 return_value_policy<reference_existing_object>())
    ;

  class_<psym_vect>("psym_vect", no_init)
    .def("__iter__", &psym_iter<psym_vect>,
	 return_value_policy<manage_new_object>())
    ;

  class_<psym_set>("psym_set", no_init)
    .def("__iter__", &psym_iter<psym_set>,
	 return_value_policy<manage_new_object>())
    ;

  class_<cgfile>("cgfile")
    .def("include", (void (cgfile::*)(char const*))&cgfile::include)
    .def("sort_psyms_by_file", &cgfile::sort_psyms_by_file)
    .def("all_program_symbols", &cgfile_binder::all_program_symbols,
	 return_value_policy<reference_existing_object>())
    .def("compute_callers", &cgfile::compute_callers)
    ;

  class_<FileSymbol>("FileSymbol", no_init)
    .add_property("name", &ProgramSymbol_binder::fsym_get_name)
    ;

  class_<ProgramSymbol>("ProgramSymbol", no_init)
    .add_property("name", &ProgramSymbol_binder::psym_get_name)
    .add_property("static", &ProgramSymbol::is_static)
    .add_property("decl", &ProgramSymbol::is_decl)
    .add_property("var", &ProgramSymbol::is_var)
    .add_property("line", &ProgramSymbol::get_line_number)
    .add_property("file", &ProgramSymbol_binder::get_file_name)
    .def("callees", &ProgramSymbol::get_callees,
	 return_value_policy<reference_existing_object>())
    .def("callers", &ProgramSymbol::get_callers,
	 return_value_policy<reference_existing_object>())
    .def("__cmp__", &ProgramSymbol_binder::cmp)
    .def("__hash__", &ProgramSymbol_binder::hash)
    .def("__repr__", &ProgramSymbol_binder::repr)
    ;
}
