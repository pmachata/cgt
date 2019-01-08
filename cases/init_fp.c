void foo(const char *(*cb)(void))
{
    const char *ptr = cb();
}

const char *(*getcb(void))(void);
void bar(void)
{
    foo(getcb());
}

struct foo
{
    const char *(*(*getcb)(void))(void);
};
void baz(struct foo *sf)
{
    foo(sf->getcb());
}
