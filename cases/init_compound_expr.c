void bar(void);
void baz(void);

static void foo (void)
{
    void (*a)(void) = ({
            struct {
                void (*cb)(void);
            } u;
            u.cb = bar;
            u.cb;
        });

    void (*b)(void) = ({
            struct {
                void (*cb)(void);
            } u;
            u.cb = baz;
            u.cb;
        });
}
