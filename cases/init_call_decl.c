int (*fp)(void);
int (*getf(void))(void);

int foo()
{
    fp = getf();
    // fp -> getf()::(ret)
    // foo -> getf
}
