void foo(void);
void *ptr = foo;
void bar(void)
{
    ((void(*)(void)) ptr)();
}
