int bar(void)
{
    return 7;
}

int baz(void)
{
    return 8;
}

int foo(int (*cb)(void))
{
    return cb();
}
