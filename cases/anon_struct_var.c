int cb(void)
{
    return 7;
}

typedef struct
{
    int (*cb)(void);
} foo_t;
typedef foo_t foo_t_t;

foo_t_t foo = {
    .cb = cb,
};

static const struct
{
    int (*cb)(void);
} s_t = {
    .cb = cb,
};


static struct
{
    int (*cb_2)(void);
} s_t_2;

void foo2(void)
{
    s_t_2.cb_2 = cb;
}

int bar(void)
{
    return s_t.cb() + s_t_2.cb_2();
}
