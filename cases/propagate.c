

struct ops {
  int (*cb)(int (*)(void));
};

int foo(int (*cb)(void)) {
  return cb();
}

int bar(void) {
  return 7;
}

int main(void) {
  struct ops o = {
     foo,
  };

  o.cb(bar);
}
