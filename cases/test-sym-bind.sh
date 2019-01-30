#!/bin/bash

source lib.sh

ACG=$(tmpfile .cg)
cat > "$ACG" <<EOF
F A.c
10100 (101) @var meh()::(arg:0) arg 0 10200 10300
10200 (102) meh
10300 (110) bar
EOF

BCG=$(tmpfile .cg)
cat > "$BCG" <<EOF
F B.c
20100 (203) @decl @var meh()::(arg:0) arg 0 20200 20300
20200 (204) @decl meh
20300 (210) baz
EOF

EXPECTCG=$(tmpfile .cg)
cat > "$EXPECTCG" <<EOF
F /tmp/A.c
1 (101) @var meh()::(arg:0) 3 4
2 (102) meh
3 (110) bar
F /tmp/B.c
4 (210) baz
EOF

expectcg $(linkcg "$ACG" "$BCG") "$EXPECTCG"
expectcg $(linkcg "$BCG" "$ACG") "$EXPECTCG"
