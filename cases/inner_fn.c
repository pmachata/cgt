static int
inner_function (int i)
{
    int innerer_function (int j) { return j; }
    return innerer_function (i);
}
