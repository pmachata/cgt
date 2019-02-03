struct s {
    int n;
};

void bar(void);

static void foo(struct s *s)
{
    // The Linux Kernel does this in kernel/bpf/core.c. We end up just
    // pretending that the callee is bar, even though it very likely isn't. But
    // I don't see a way to figure out the set of functions that this could
    // load.
    (bar + s->n)();
}
