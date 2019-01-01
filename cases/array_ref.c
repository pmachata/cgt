typedef int (*cb_t)(void);
struct ops {
    cb_t cbs[2];
};

int a(void) {return 1;}
int b(void) {return 2;}
int c(void) {return 3;}
int d(void) {return 4;}
int e(void) {return 5;}
int f(void) {return 6;}

struct ops o = {
    .cbs = {
        a,
        b,
    },
};
// ops::cbs[] -> a
// ops::cbs[] -> b

struct ops *get_pops(void)
{
    static struct ops ret = {
        .cbs = {
            c,
            d,
        },
    };
    return &ret;
}
// ops::cbs[] -> c
// ops::cbs[] -> d

struct ops get_ops(void)
{
    return (struct ops) {
        .cbs = {
            e,
            f,
        },
    };
}
// ops::cbs[] -> e
// ops::cbs[] -> f

void doit(struct ops *ops) {
    cb_t cb = ops->cbs[0];
    cb();
}
// doit -> ops::cbs[]

int main(int argc, char **argv)
{
    if (argc == 1)
        doit(&o);
    else if (argc == 2)
        doit(get_pops());
    else if (argc == 3) {
        struct ops oo = get_ops();
        doit(&oo);
    }
}
// main -> doit
// main -> get_pops
// main -> get_ops
