struct ops {
    int (*pf)(const char *fmt, ...);
};

static struct ops main_ops = {
    .pf = 0,
};

void f(void) {}
