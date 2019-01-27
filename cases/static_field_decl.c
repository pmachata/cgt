static void foo(void)
{
    struct s {
        void (*cb)(void);
    } s = {foo};
}

void goo(void)
{
    struct s {
        void (*cb)(void);
    } s = {goo};
}

static struct
{
    void (*cb)(void);
} f = {foo};

struct
{
    void (*cb)(void);
} g = {goo};

void outer(void)
{
    void inner(void)
    {
        static struct
        {
            void (*cb)(void);
        } f = {outer};
        struct
        {
            void (*cb)(void);
        } g = {outer};
    }
}
