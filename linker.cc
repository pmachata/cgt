#include "cgfile.hh"
#include "quark.hh"
#include "reader.hh"
#include "symbol.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>

static void
link(char ** filenames, int count, cgfile & f)
{
  tok_vect_vect file_tokens;

  for (int k = 0; k < count; ++k)
    {
      char const* curmodule = filenames[k];

      fd_reader *rd = open_or_die(curmodule);
      tokenize_file(rd, file_tokens);
      f.include(file_tokens, curmodule);
      delete rd;
    }
}

int
main(int argc, char **argv)
{
  char const* output = NULL;
  int opt;

  while ((opt = getopt(argc, argv, "ho:")) != -1)
    {
      switch (opt) {
      case 'o':
	if (output == NULL)
	  output = optarg;
	else
	  std::cerr << "-o already specified." << std::endl;
	break;
      case 'h':
      default:
	printf("usage: linker [files and options]\n");
	printf("  -o <file>     output to file (stdout by default)\n");
	printf("  -h	        print usage\n");
	return 0;
      }
    }

  if (optind >= argc)
    {
      std::cerr << "Need input files." << std::endl;
      return 1;
    }

  std::ofstream outfile;
  std::ostream &outs = output == NULL ? std::cout
    : (outfile.open(output), outfile);
  check_stream(outs, output);

  cgfile f;
  link(argv + optind, argc - optind, f);
  f.sort_psyms_by_file();

  q::Quark path = NULL;
  for (psym_vect::const_iterator it = f.all_program_symbols.begin();
       it != f.all_program_symbols.end(); ++it)
    {
      ProgramSymbol * psym = *it;
      if (psym->is_forwarder() || !psym->is_used())
	continue;

      q::Quark psym_path = psym->get_qpath();
      if (psym_path != path)
	{
	  path = psym_path;
	  if (path == NULL)
	    {
	      std::cerr << "warning unset path for symbol " << std::flush;
	      std::cerr << psym->get_name() << ": " << std::flush;
	      std::cerr << psym->get_file()->get_name() << "." << std::endl;
	    }
	  outs << "F " << psym->get_path() << std::endl;
	}

      psym->dump(outs);
    }
}
