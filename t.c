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

struct bit_field_ref {
    int has_children : 1;
};

static int
bit_field_ref (struct bit_field_ref *bfr)
{
    return bfr->has_children ? 10 : 20;
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
