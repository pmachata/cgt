void initializer_null_cb (int (*fn)(void))
{
}

void initializer_null(void)
{
    initializer_null_cb(0);
}
