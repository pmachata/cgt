#if defined likely && !defined USE_EXPECT
# undef likely
# undef unlikely
#endif
#if !defined likely
# if defined USE_EXPECT
#  define unlikely(expr) __builtin_expect (!!(expr), 0)
#  define likely(expr) __builtin_expect (!!(expr), 1)
# else
#  define unlikely(expr) expr
#  define likely(expr) expr
# endif
#endif
