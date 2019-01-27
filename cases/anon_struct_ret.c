void bar(void) {
}

struct {
    void (*fp)(void);
}
foo(void)
{
    return (__typeof__(foo())){bar};
}

void
baz(void)
{
    foo().fp();
}
