#include "cgfile.hh"
#include "reader.hh"
#include "symbol.hh"
#include "rand.hh"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cctype>

template< class T >
std::string
to_string( T x )
{
  std::ostringstream o;
  o << x;
  return o.str();
}

int
main(int argc, char **argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "h")) != -1)
    {
      switch (opt) {
      case 'h':
      default:
	// XXX would be cool to have a couple options:
	//  - number of files
	//  - % of external symbols, variables, etc.
	//  - dictionary to use, or no dictionary
	std::cout << "usage: query <output file> <#symbols> <#edges>" << std::endl
		  << "  -h	        print usage" << std::endl;
	return 0;
      }
    }

  if (optind >= argc - 2)
    {
      std::cerr << "Need output file and callgraph data." << std::endl;
      return 1;
    }

  char *filename = argv[optind++];
  char *symbols = argv[optind++];
  char *edges = argv[optind++];

  std::ofstream outfile;
  outfile.open(filename);
  check_stream(outfile, filename);

  std::vector<std::string> words;
  tok_vect_vect file_tokens;
  fd_reader rd {std::move (open_or_die("/usr/share/dict/words"))};
  tokenize_file(&rd, file_tokens);
  for (tok_vect_vect::const_iterator it = file_tokens.begin();
       it != file_tokens.end(); ++it)
    {
      tok_vect const& line = *it;
      if (line.size() == 0 || line.size() > 1)
	continue;
      std::string word = line[0];
      if (word.length() == 0
	  || (!isalpha(word[0]) && word[0] != '_'))
	continue;
      for (size_t i = 1; i < word.length(); ++i)
	if (!isalnum(word[i]) && word[i] != '_')
	  goto skip;

      words.push_back(word);

    skip:
      ;
    }

  std::cerr << words.size() << " words" << std::endl;

  psym_vect all_symbols;
  FileSymbol *fsym = new FileSymbol("somefile.c");
  unsigned long line = 0;
  for (int i = 0; i < atoi(symbols); ++i)
    {
      line += static_cast<unsigned long>(cg::rand() * 50);
      unsigned long w = static_cast<unsigned long>(cg::rand() * words.size());
      std::string word = words[w] + '.' + to_string(i);
      auto psym = all_symbols.emplace_back
                    (std::make_unique<ProgramSymbol>(word, fsym, line)).get();
      psym->set_static(cg::rand() > 0.5);
      psym->set_decl(cg::rand() > 0.5);
      psym->set_var(cg::rand() > 0.5);
      psym->set_path(fsym->get_name());
    }

  for (int i = 0; i < atoi(edges); ++i)
    {
      ProgramSymbol *psym1 = NULL, *psym2 = NULL;
      do {
	unsigned long a = static_cast<unsigned long>(cg::rand() * all_symbols.size());
	unsigned long b = static_cast<unsigned long>(cg::rand() * all_symbols.size());
	psym1 = all_symbols[a].get();
	psym2 = all_symbols[b].get();
      } while (psym1->get_callees().find(psym2) != psym1->get_callees().end());
      psym1->add_callee(psym2);
    }

  std::string path;
  for (auto const &psym: all_symbols)
    {
      if (auto psym_path = psym->get_path();
          psym_path != path)
        {
          path = psym_path;
	  if (path == "")
	    {
	      std::cerr << "warning unset path for symbol " << std::flush;
	      std::cerr << psym->get_name() << ": " << std::flush;
	      std::cerr << psym->get_file()->get_name() << "." << std::endl;
	    }
	  outfile << "F " << psym_path << std::endl;
	}

      psym->dump(outfile);
    }
}
