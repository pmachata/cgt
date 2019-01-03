struct q {
    int (*foo)();
    int (*bar)(void);
};

int foo() {}
int bar(void) {}

int main(void)
{
    struct q q;
    q.foo();
    q.bar();
}
