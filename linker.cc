#include "cgfile.hh"
#include "reader.hh"
#include "symbol.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <fstream>
#include <iostream>

static void
link(char ** filenames, int count, cgfile & f)
{
  tok_vect_vect file_tokens;

  for (int k = 0; k < count; ++k)
    {
      char const* curmodule = filenames[k];

      fd_reader *rd;
      try {
        rd = new fd_reader(FD(curmodule));
      }
      catch (int) {
        std::cerr << "Error opening " << curmodule
              << " for reading, will be ignored..."
              << std::endl;
        continue;
      }
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
      std::cerr << argv[0] << ": need input files." << std::endl;
      return 1;
    }

  std::ofstream outfile;
  std::ostream &outs = output == NULL ? std::cout
    : (outfile.open(output), outfile);
  check_stream(outs, output);

  cgfile f;
  link(argv + optind, argc - optind, f);
  f.sort_psyms_by_file();
  f.compute_used();
  f.dump(outs);
}
