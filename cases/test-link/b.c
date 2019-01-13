int baz(void);
int foo(int (*cb)(void));

int twain(void) {
    foo(baz);
}
