typedef struct
{
    int (*cb)(void);
} s_t;

int cb(void)
{
    return 7;
}

void foo(s_t *ptr)
{
    ptr->cb = cb;
}

int bar(s_t *ptr)
{
    return ptr->cb ();
}
