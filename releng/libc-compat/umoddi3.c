typedef unsigned long long      uint64_t;
typedef signed long long        int64_t;

uint64_t
__udivmoddi4 (uint64_t num, uint64_t den, uint64_t *rem_p)
{
  uint64_t quot = 0, qbit = 1;

  if (den == 0)
  {
    return 1 / ((unsigned) den); /* Intentional divide by zero, without
                                 triggering a compiler warning which
                                 would abort the build */
  }

  /* Left-justify denominator and count shift */
  while ((int64_t) den >= 0)
  {
    den <<= 1;
    qbit <<= 1;
  }

  while (qbit)
  {
    if (den <= num)
    {
      num -= den;
      quot += qbit;
    }
    den >>= 1;
    qbit >>= 1;
  }

  if (rem_p != NULL)
    *rem_p = num;

  return quot;
}

uint64_t
__umoddi3 (uint64_t num, uint64_t den)
{
  uint64_t v;

  (void) __udivmoddi4 (num, den, &v);
  return v;
}
