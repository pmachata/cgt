void bar(void);
static void foo(void (*cb)(void))
{
    cb = bar;
    cb();
}
