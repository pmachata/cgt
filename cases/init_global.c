struct foo
{
    int (*debugscn_p)(const char *);
};

extern int (*generic_debugscn_p) (const char *);

void
foo (struct foo *eh)
{
  generic_debugscn_p = eh->debugscn_p;
}
