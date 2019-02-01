static inline void foo(void)
{
    union {
        struct {
            void (*cb)(void);
        } l;
    } a, b;
    a.l.cb = foo;
}
