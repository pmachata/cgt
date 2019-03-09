typedef void (*fn1_t)(int);

int fn3(void);
int fn4(void);
fn1_t foo(int i);

void call_cond_expr_1(void)
{
    foo(fn3())(fn4());
}
