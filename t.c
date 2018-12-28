static inline void
target_expr (int width)
{
    ({
        width;
        width;
    });
}

static inline void
asm_expr (void)
{
    const unsigned char *Addr;
    unsigned short int __v, __x = (unsigned short int) (*Addr);
    __asm__ ("rorw $8, %w0"
             : "=r" (__v)
             : "0" (__x)
             : "cc");
}

typedef void (*__attribute__ ((noreturn)) Dwarf_OOM) (void);

struct Dwarf
{
  Dwarf_OOM oom_handler;
};

Dwarf_OOM
dwarf_new_oom_handler (struct Dwarf *dbg, Dwarf_OOM handler)
{
  Dwarf_OOM old = dbg->oom_handler;
  dbg->oom_handler = handler;
  return old;
}
