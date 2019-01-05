typedef struct
{
    int (*cb)(void);
} s_t;

typedef s_t s_t_t;

int cb(void)
{
    return 7;
}

void foo(s_t_t *ptr)
{
    ptr->cb = cb;
}

int bar(s_t *ptr)
{
    return ptr->cb ();
}
