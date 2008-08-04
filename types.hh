#if defined __GXX_EXPERIMENTAL_CXX0X__ || defined USE_CPP0X
# include <unordered_map>
# include <unordered_set>
# define MAP unordered_map
# define SET unordered_set
#else
# include <map>
# include <set>
# define MAP map
# define SET set
#endif

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
