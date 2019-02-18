void bar(void);

static void foo (void)
{
  ({
    struct {
        void (*cb)(void);
    } u;
    u.cb = bar;
    u.cb;
  })();
}
