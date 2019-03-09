typedef void (*fn1_t)(void);
typedef fn1_t (*fn2_t)(void);
typedef fn2_t (*fn3_t)(void);

void fn1(void);
fn1_t fn2(void) {
    return &fn1;
    // fn2::ret -> fn1
}
fn2_t fn3(void) {
    return &fn2;
    // fn3::ret -> fn2
}
fn3_t fn4(void) {
    return &fn3;
    // fn4::ret -> fn3
}

static fn1_t call_cond_expr_callee_1(fn2_t fn) {
    return fn();
    // call_cond_expr_callee_1 -> call_cond_expr_callee_1::arg0
    // call_cond_expr_callee_1::ret -> call_cond_expr_callee_1::arg0::ret
}
static fn1_t call_cond_expr_callee_1a(fn2_t fn) {
    return fn();
    // call_cond_expr_callee_1a -> call_cond_expr_callee_1a::arg0
    // call_cond_expr_callee_1a::ret -> call_cond_expr_callee_1a::arg0::ret
}
static fn1_t call_cond_expr_callee_2(fn3_t fn) {
    return fn()();
    // call_cond_expr_callee_2 -> call_cond_expr_callee_2::arg0
    // call_cond_expr_callee_2 -> call_cond_expr_callee_2::arg0::ret
    // call_cond_expr_callee_2::ret -> call_cond_expr_callee_1::arg0::ret::ret
}
static int call_cond_expr_dispatch(int i)
{
    return i;
}
void
call_cond_expr_1(int arg)
{
    call_cond_expr_callee_1((arg ? fn3 : fn4())())();
    // call_cond_expr_1 -> call_cond_expr_callee_1
    // call_cond_expr_1 -> call_cond_expr_callee_1::ret
    // call_cond_expr_1 -> fn4
    // call_cond_expr_1 -> fn3
    // call_cond_expr_1 -> fn4::ret
    // call_cond_expr_callee_1::arg0 -> fn3::ret
    // call_cond_expr_callee_1::arg0 -> fn4::ret::ret
}
void
call_cond_expr_2(int arg)
{
    (call_cond_expr_dispatch(arg)
     ? call_cond_expr_callee_1a(fn2) : call_cond_expr_callee_2(fn3))();
    // call_cond_expr_2 -> call_cond_expr_dispatch
    // call_cond_expr_2 -> call_cond_expr_callee_1a
    // call_cond_expr_2 -> call_cond_expr_callee_2
    // call_cond_expr_2 -> call_cond_expr_callee_1a::ret
    // call_cond_expr_2 -> call_cond_expr_callee_2::ret
    // call_cond_expr_callee_1a::arg0 -> fn2
    // call_cond_expr_callee_2::arg0 -> fn3
}
