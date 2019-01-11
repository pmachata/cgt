int bar(void);
int foo(int (*cb)(void));

int baz(void) {
    return foo(bar);
}
