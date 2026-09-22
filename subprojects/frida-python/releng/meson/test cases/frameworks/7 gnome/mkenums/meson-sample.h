#pragma once

typedef enum
{
    MESON_THE_XVALUE,
    MESON_ANOTHER_VALUE
} MesonTheXEnum;

typedef enum /*< skip >*/
{
    MESON_FOO
} MesonThisEnumWillBeSkipped;

typedef enum /*< flags,prefix=MESON >*/
{
    MESON_THE_ZEROTH_VALUE,  /*< skip >*/
    MESON_THE_FIRST_VALUE,
    MESON_THE_SECOND_VALUE,
    MESON_THE_THIRD_VALUE,   /*< nick=the-last-value >*/
} MesonTheFlagsEnum;
