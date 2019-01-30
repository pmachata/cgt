struct s {
    void (**cb)(void);
};
void bar(void);
int foo(struct s *s)
{
    s->cb[4] = bar;
}
