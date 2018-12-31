extern int printf(const char *fmt, ...);
extern int atoi(const char *);

int printf2(const char *fmt, ...)
{
    printf("fmt:%s\n", fmt);
} // printf2 -> printf

struct ops {
    int (*pf)(const char *fmt, ...);
};

int
foo(struct ops *ops, int k)
{
    return ops->pf("[%d]\n", k);
}
// foo -> ops::pf

const char *
last_arg(int argc, char **argv)
{
    return argv[argc - 1];
}

const char *
first_arg(int argc, char **argv)
{
    return argv[0];
}

const char *(*get_get_arg (int i,
                           const char *(*dflt)(int, char **)))(int, char **)
{
    const char *(*gn)(int, char **) = dflt;
    const char *(*fn)(int, char **);

    if (i <= 1)
        fn = gn;
    else
        fn = &last_arg;

    if (i > 10)
        fn = gn;

    return fn;
    // internal: gn -> get_get_arg()::dflt
    // internal: fn -> gn
    // internal: fn -> last_arg
    // internal: get_get_arg()::<ret> -> fn
}
// get_get_arg()::<ret> -> last_arg
// get_get_arg()::<ret> -> get_get_arg()::dflt

int
call_foo(struct ops *ops,
         const char *(*get_arg)(int, char **),
         int argc, char **argv)
{
    const char *arg = get_arg(argc, argv);
    int i = atoi(arg);
    return foo(ops, i);
}
// call_foo -> call_foo()::get_arg
// call_foo -> atoi
// call_foo -> foo

static struct ops main_ops = {
    .pf = printf,
};
// ops::pf -> printf

struct ops *
get_twain_ops(void)
{
    static struct ops twain_ops;
    twain_ops.pf = printf2;
    return &twain_ops;
}
// ops::pf -> printf2

int
call_foo_main(const char *(*get_arg)(int, char **),
              int argc, char **argv)
{
    if (argc > 5)
        return call_foo(&main_ops, get_arg, argc, argv);
    else
        return call_foo(get_twain_ops (), get_arg, argc, argv);
}
// call_foo_main -> call_foo
// call_foo_main -> get_twain_ops
// call_foo()::get_arg -> call_foo_main()::get_arg

int
main(int argc, char *argv[])
{
    return call_foo_main(get_get_arg (argc, first_arg), argc, argv);
}
// main -> get_get_arg
// main -> call_foo_main
// call_foo_main()::get_arg -> get_get_arg()::<ret>
// get_get_arg()::dflt -> first_arg
