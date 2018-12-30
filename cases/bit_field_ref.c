struct bit_field_ref {
    int has_children : 1;
};

static int
bit_field_ref (struct bit_field_ref *bfr)
{
    return bfr->has_children ? 10 : 20;
}
