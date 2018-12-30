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
