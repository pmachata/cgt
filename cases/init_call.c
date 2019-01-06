int (*fp)(void);

int f(void)
{
    return 7;
}

int (*getf(void))(void)
{
    return f;
}

int foo()
{
    fp = getf();
    // fp -> getf()::(ret)
    // foo -> getf
}
