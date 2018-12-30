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
