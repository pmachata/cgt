static int
imagpart_expr (void)
{
  unsigned int phnum = 10;
  unsigned long long phdr_size = 10;
  return phnum > 18446744073709551615UL / phdr_size;
}
