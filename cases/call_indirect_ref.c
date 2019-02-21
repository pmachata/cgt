void foo(void (**fp)(void)) {
    (*fp)();
}
void bar(int i, void (**fp)(void)) {
    (fp[i])();
}
int baz(void (*fp)(void)) {
    foo(&fp);
    bar(0, &fp);
}
