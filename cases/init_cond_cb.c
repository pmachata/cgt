typedef int (*DisasmGetSymCB_t) (void);

struct ebl {
    int (*disasm) (DisasmGetSymCB_t symcb);
};

struct ctx {
  struct ebl *ebl;
  DisasmGetSymCB_t symcb;
};

static int null_elf_getsym (void)
{
  return -1;
}

int foo (struct ctx *ctx)
{
  DisasmGetSymCB_t getsym = ctx->symcb ?: null_elf_getsym;

  return ctx->ebl->disasm (getsym);
}
