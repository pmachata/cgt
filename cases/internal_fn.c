void
foo (int i)
{
    switch (i) {
    case 0:
        __attribute__ ((fallthrough));
    case 1:
        break;
    }
}
