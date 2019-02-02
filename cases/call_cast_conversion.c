void foo(void);
unsigned long ptr = (unsigned long) &foo;
void bar(void)
{
    ((void(*)(void)) ptr)();
}
