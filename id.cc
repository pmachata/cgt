#include "id.hh"

namespace {
  unsigned id = 0;
}

unsigned gen_id() {
  return ++id;
}
