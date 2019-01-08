int f(void) { return 7; }

int (*fp1)(void);
int (*fp2)(void);

int foo(void)
{
    return fp1() + fp2();
}

int main(void)
{
    fp1 = fp2 = f;
}

// and make a cg out of me when you do
