typedef int (*DisasmGetSymCB_t) (void);

int disasm (DisasmGetSymCB_t symcb)
{
}

struct ctx {
  DisasmGetSymCB_t symcb;
};

static int null_elf_getsym (void)
{
  return -1;
}

int foo (struct ctx *ctx)
{
  DisasmGetSymCB_t getsym = ctx->symcb ?: null_elf_getsym;

  return disasm (getsym);
}
