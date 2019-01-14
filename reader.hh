#ifndef cgt_reader_hh_guard
#define cgt_reader_hh_guard

#include <unistd.h>
#include <vector>
#include <iosfwd>

struct FD {
  FD(char const* name);
  ~FD();

  operator int() const {
    return m_fd;
  }

private:
  FD(FD const& deleted);
  FD& operator=(FD const& deleted);
  int m_fd;
};

class fd_reader {
  size_t m_cursor;
  size_t m_size;
  char *m_buffer;

public:
  fd_reader(FD const& fd);
  fd_reader(fd_reader &&move);
  ~fd_reader();
  char* getline();

  bool eof() const {
    return m_cursor >= m_size
      || m_buffer == NULL;
  }
};

typedef std::vector<char const*> tok_vect;
void tokenize_line(char* line, tok_vect &vector);

typedef std::vector<tok_vect> tok_vect_vect;
void tokenize_file(fd_reader *rd, tok_vect_vect &tokens);

fd_reader open_or_die(char const* filename);
void check_stream(std::ios const& ios, char const* filename);

#endif//cgt_reader_hh_guard
