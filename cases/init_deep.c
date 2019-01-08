int f(void) { return 7; }

struct bar {
    int (*fp)(void);
};

struct foo {
    struct bar bar;
} foo = {{.fp = f }};
// bar::fp -> f
