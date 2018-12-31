int foo (void);
extern __typeof__ (foo) __foo_internal __attribute__ ((visibility ("hidden")));

int
foo (void)
{}
extern __typeof__ (foo) __foo_internal __attribute__ ((alias ("foo")));
