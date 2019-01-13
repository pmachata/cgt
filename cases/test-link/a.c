int bar(void);
int foo(int (*cb)(void));

int main(void) {
    foo(bar);
}
