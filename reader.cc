#include "reader.hh"
#include "types.hh"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

FD::FD(char const* name)
  : m_fd(open(name, O_RDONLY))
{
  if (unlikely (m_fd <= 0))
    throw errno;
}

FD::~FD()
{
  close(m_fd);
}


namespace {
  size_t
  getsize(int fd)
  {
    struct stat sb;
    if (unlikely (fstat(fd, &sb) != 0))
      throw errno;
    return static_cast<size_t>(sb.st_size);
  }
}

fd_reader::fd_reader(FD const& fd)
  : m_cursor(0)
  , m_size(::getsize(fd))
  , m_buffer(m_size == 0 ? NULL
	     : static_cast<char*>(mmap(NULL, m_size, PROT_READ|PROT_WRITE,
				       MAP_PRIVATE, fd, 0)))
{
  if (unlikely (m_buffer == MAP_FAILED))
    throw errno;
}

fd_reader::fd_reader(fd_reader &&move)
  : m_cursor {move.m_cursor}
  , m_size {move.m_size}
  , m_buffer {move.m_buffer}
{
  move.m_buffer = nullptr;
}

char*
fd_reader::getline()
{
  if (m_cursor >= m_size)
    return NULL;

  size_t remsize = m_size - m_cursor;
  char *start = m_buffer + m_cursor;
  char *end = static_cast<char*>(std::memchr(start, '\n', remsize));
  if (end == NULL)
    return NULL;

  *end = 0;
  char *ret = start;
  m_cursor = end - m_buffer + 1;
  return ret;
}

fd_reader::~fd_reader()
{
  munmap(static_cast<void*>(m_buffer), m_size);
}

void
tokenize_line(char* line, tok_vect &vector)
{
  size_t len = std::strlen(line);
  char* end = line + len;
  char* pos1 = line;
  char* pos2;

  while (pos1 < end && (pos2 = std::strchr(pos1, ' ')) != NULL)
    {
      *pos2 = 0;
      vector.push_back(pos1);
      for (pos1 = pos2 + 1; *pos1 == ' '; ++pos1)
	;
    }

  if (pos2 == NULL)
    vector.push_back(pos1);
}

void
tokenize_file(fd_reader *rd, tok_vect_vect &tokens)
{
  tokens.clear();
  char *line;
  while ((line = rd->getline()))
    {
      tokens.push_back(tok_vect());
      tok_vect &curtoks = tokens.back();

      *strchrnul(line, '#') = 0;
      if (*line == 0)
	continue;

      tokenize_line(line, curtoks);
      assert (curtoks.size() >= 1);
    }
}

fd_reader
open_or_die(char const* filename)
{
  try {
    return fd_reader(FD(filename));
  }
  catch (int code) {
    std::cerr << "Error opening "
	      << filename << " for reading." << std::endl;
    std::exit(1);
  }
}

void
check_stream(std::ios const& ios, char const* filename)
{
  if (unlikely (!ios.good()))
    {
      std::cerr << "Error opening output file `"
		<< filename << "': " << std::strerror(errno)
		<< "." << std::endl;
      std::exit(1);
    }
}
