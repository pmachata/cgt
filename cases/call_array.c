void f(void) {}
void g(void) {}

void (*fps[])(void) = {f, g};

void foo(void) {
    (*fps[0]) ();
}
