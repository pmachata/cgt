static void call_cond_expr_callee_1(void) {}
static void call_cond_expr_callee_2(void) {}
static int call_cond_expr_dispatch(int i)
{
    return i;
}
static void
call_cond_expr_1 (int arg)
{
    (arg ? call_cond_expr_callee_1 : call_cond_expr_callee_2)();
}
static void
call_cond_expr_2 (int arg)
{
    (call_cond_expr_dispatch (arg)
     ? call_cond_expr_callee_1 : call_cond_expr_callee_2)();
}

typedef void (*fn1_t)(void);

void fn1(void);
fn1_t fn2(void);

void foo(int arg)
{
    (arg ? fn2() : fn1)();
}
