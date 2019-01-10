//: -O2
struct foo {
  int st_name;
  char st_other;
  short st_shndx;
};

int fn(struct foo *sym1, struct foo *sym2)
{
    return sym1->st_other != sym2->st_other
        || sym1->st_shndx != sym2->st_shndx;
}
