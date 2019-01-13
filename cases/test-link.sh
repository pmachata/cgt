#!/bin/bash

source lib.sh

A=$(cfile)
cat >"$A" <<EOF
int bar(void);
int foo(int (*cb)(void));

int main(void) {
    foo(bar);
}
EOF

B=$(cfile)
cat >"$B" <<EOF
int baz(void);
int foo(int (*cb)(void));

int twain(void) {
    foo(baz);
}
EOF

C=$(cfile)
cat >"$C" <<EOF
int bar(void)
{
    return 7;
}

int baz(void)
{
    return 8;
}

int foo(int (*cb)(void))
{
    return cb();
}
EOF

CG=$(tmpfile .cg)
cat >"$CG" <<EOF
1 (0) *
F /tmp/A.c
4 (2) @decl foo()::(arg:0) 2 6
5 (4) main 3
F /tmp/C.c
2 (1) bar
3 (11) foo 4
6 (6) baz
F /tmp/B.c
7 (4) twain 3
EOF

ACG=$(buildcg "$A" A.c)
BCG=$(buildcg "$B" B.c)
CCG=$(buildcg "$C" C.c)
TEST_CG=$(linkcg "$ACG" "$BCG" "$CCG")
expectcg "$TEST_CG" "$CG"
