# A file to keep the list of temporary files. We can't use an array for this,
# because tmpfile is invoked in a subshell.
TMPFILES=$(mktemp)

if [ -z "$CC" ]; then
    CC=gcc
fi

CALGARY_SO=$(realpath $(pwd)/../calgary.so)

tmpfile()
{
    local suff=$1; shift

    local TMP=$(mktemp XXXXXXXXXX"$suff" --tmpdir)
    echo "$TMP" >> "$TMPFILES"
    echo "$TMP"
}

cfile()
{
    tmpfile .c
}

buildcg()
{
    local cfile=$1; shift
    local name=$1; shift

    local cgfile=$(tmpfile .cg)
    local ofile=$(tmpfile .o)
    local cflags=$(grep ^//: "$cfile" | cut -d' ' -f 2-)

    "$CC" -c "$cfile" $cflags -fplugin="$CALGARY_SO" -o "$ofile"
    objcopy --dump-section=.calgary.callgraph="$cgfile" "$ofile"
    sed -i "s|$cfile|$name|" "$cgfile"

    echo "$cgfile"
}

linkcg()
{
    local cgfiles=("${@}")

    local cgfile=$(tmpfile .cg)
    ../linker "${cgfiles[@]}" -o "$cgfile"
    echo "$cgfile"
}

expectcg()
{
    local testcg=$1; shift
    local expectcg=$1; shift

    diff -u "$expectcg" "$testcg" || exit 1
}

cleanup()
{
    cat "$TMPFILES" | while read a; do
        rm -f "$a"
    done
    rm -f "$TMPFILES"
}

trap cleanup EXIT
