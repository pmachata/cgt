#!/bin/bash

source lib.sh

ACG=$(buildcg test-link/a.c A.c)
BCG=$(buildcg test-link/b.c B.c)
CCG=$(buildcg test-link/c.c C.c)
TEST_CG=$(linkcg "$ACG" "$BCG" "$CCG")
expectcg "$TEST_CG" test-link/cg
