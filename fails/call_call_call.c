typedef void (*fn1_t)(void);
typedef fn1_t (*fn2_t)(void);
typedef fn2_t (*fn3_t)(void);

void fn1(void);
fn1_t fn2(void) {
    return &fn1;
    // fn2::(ret) -> fn1
}
fn2_t fn3(void) {
    return &fn2;
    // fn3::(ret) -> fn2
}
fn3_t fn4(void) {
    return &fn3;
    // fn4::(ret) -> fn3
}

void foo(void) {
    fn4()()()();
    // foo -> fn4
    // foo -> fn4::(ret)
    // foo -> fn4::(ret)::(ret)
    // foo -> fn4::(ret)::(ret)::(ret)
}

// And the linker will have to resolve this to:
// foo -> fn4
// fn4::(ret)::(ret) -> fn3::(ret)
// fn4::(ret)::(ret)::(ret) -> fn2::(ret)
