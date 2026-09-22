#if defined(MESON_TEST__UNDERSCORE_SYMBOL)
# define SYMBOL_NAME(name) _##name
#else
# define SYMBOL_NAME(name) name
#endif
