extern void f(void);
extern void g(void);
extern void h(void);
extern void undefined(void);

/* Defined in nope.c and f.c,
 * value depends on the source set and configuration used.
 */
extern void (*p)(void);
