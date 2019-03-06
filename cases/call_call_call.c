typedef void (*fn1_t)(void);
typedef fn1_t (*fn2_t)(void);
typedef fn2_t (*fn3_t)(void);

void fn1(void);
fn1_t fn2(void) {
    return &fn1;
}
fn2_t fn3(void) {
    return &fn2;
}
fn3_t fn4(void) {
    return &fn3;
}

void foo(void) {
    fn4()()()();
}

void foo2(int i) {
    (i ? fn4()() : fn3())()();
    // foo2 -> fn4
    // foo2 -> fn4::ret
    // foo2 -> fn3
    // --- ^ternary^ ---
    // foo2 -> fn4::ret::ret
    // foo2 -> fn3::ret
    // --- ^first call^ ---
    // foo2 -> fn4::ret::ret::ret
    // foo2 -> fn3::ret::ret
    // --- ^second call^ ---
}

void foo3(int i, fn1_t (*cb)(void))
{
    cb()();
    // foo3 -> foo3::cb
    // foo3 -> foo3::cb::ret
}

struct s {
    fn1_t cb;
} ss;
void foo4(void)
{
    ss.cb = fn3()();
}

int main(void) {
    foo3(0, fn2);
}
