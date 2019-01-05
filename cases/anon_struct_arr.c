int ab(void)
{
    return 5;
}

int cb(void)
{
    return 7;
}

static const struct
{
    int (*cb)(void);
} s_t[] = {
    { .cb = ab },
    { .cb = cb },
};

int bar(void)
{
    return s_t[0].cb() + s_t[1].cb();
}
