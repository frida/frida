/*
 * Copyright (C) 2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include <glib.h>

extern "C"
{
  void test_check_bit (guint32 * val, guint8 bit);
  void test_set_bit (const char * func, guint32 * val, guint8 bit);
  void test_try_and_catch (guint32 * val);
  void test_try_and_dont_catch (guint32 * val);
}

class TestException;
class FakeException;
class TestResource;

static void test_try_and_catch_pp (guint32 * val);
static void test_try_and_dont_catch_pp (guint32 * val);
static void test_try_and_dont_catch_pp_2 (guint32 * val);
static void test_try_and_dont_catch_pp_3 (guint32 * val);

extern "C"
{
  void
  test_check_bit (guint32 * val,
                  guint8 bit)
  {
    g_assert_true ((*val & (1U << bit)) != 0);
    *val &= ~1U << bit;
  }

  void
  test_set_bit (const char * func,
                guint32 * val,
                guint8 bit)
  {
    if (g_test_verbose ())
      g_print ("\tFunc: %s, Set: %d\n", func, bit);

    *val |= 1U << bit;
  }

  void
  test_try_and_catch (guint32 * val)
  {
    test_try_and_catch_pp (val);
  }

  void
  test_try_and_dont_catch (guint32 * val)
  {
    test_try_and_dont_catch_pp (val);
  }
}

class TestException
{
};

class FakeException
{
};

class TestResource
{
public:
  TestResource (guint32 * val)
    : val (val)
  {
    test_set_bit ("TestResource", val, 0);
  }

  ~TestResource ()
  {
    test_set_bit ("TestResource", val, 1);
  }

private:
  guint32 * val;
};

static void
test_try_and_catch_pp (guint32 * val)
{
  try
  {
    test_set_bit ("test_try_and_catch_pp", val, 0);

    throw TestException ();

    test_set_bit ("test_try_and_catch_pp", val, 1);
  }
  catch (TestException & ex)
  {
    test_set_bit ("test_try_and_catch_pp", val, 2);
  }

  test_set_bit ("test_try_and_catch_pp", val, 3);
}

static void
test_try_and_dont_catch_pp (guint32 * val)
{
  try
  {
    test_set_bit ("test_try_and_dont_catch_pp", val, 2);

    test_try_and_dont_catch_pp_2 (val);

    test_set_bit ("test_try_and_dont_catch_pp", val, 3);
  }
  catch (FakeException &)
  {
    test_set_bit ("test_try_and_dont_catch_pp", val, 4);
  }
  catch (TestException &)
  {
    test_set_bit ("test_try_and_dont_catch_pp", val, 5);
  }

  test_set_bit ("test_try_and_dont_catch_pp", val, 6);
}

static void
test_try_and_dont_catch_pp_2 (guint32 * val)
{
  TestResource x (val);

  try
  {
    test_set_bit ("test_try_and_dont_catch_pp_2", val, 7);

    test_try_and_dont_catch_pp_3 (val);

    test_set_bit ("test_try_and_dont_catch_pp_2", val, 8);
  }
  catch (FakeException &)
  {
    test_set_bit ("test_try_and_dont_catch_pp_2", val, 9);
  }

  test_set_bit ("test_try_and_dont_catch_pp_2", val, 10);
}

static void
test_try_and_dont_catch_pp_3 (guint32 * val)
{
  test_set_bit ("test_try_and_dont_catch_pp_3", val, 11);

  throw TestException ();

  test_set_bit ("test_try_and_dont_catch_pp_3", val, 12);
}
