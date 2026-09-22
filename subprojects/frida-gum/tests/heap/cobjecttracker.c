/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "cobjecttracker-fixture.c"

#ifdef HAVE_WINDOWS

TESTLIST_BEGIN (cobjecttracker)
  TESTENTRY (total_count_increase)
  TESTENTRY (total_count_decrease)
  TESTENTRY (object_list)
TESTLIST_END ()

TESTCASE (total_count_increase)
{
  GumCObjectTracker * t = fixture->tracker;

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 0);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 0);
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "MyObject"),
      ==, 0);
  fixture->ht1 = g_hash_table_new (NULL, NULL);
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 1);
  fixture->ht2 = g_hash_table_new (NULL, NULL);
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 2);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 2);

  fixture->mo = my_object_new ();
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "MyObject"),
      ==, 1);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 3);
}

TESTCASE (total_count_decrease)
{
  GumCObjectTracker * t = fixture->tracker;

  fixture->ht1 = g_hash_table_new (NULL, NULL);
  fixture->ht2 = g_hash_table_new (NULL, NULL);
  fixture->mo = my_object_new ();

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 3);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 2);
  g_hash_table_unref (fixture->ht1); fixture->ht1 = NULL;
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 1);
  g_hash_table_unref (fixture->ht2); fixture->ht2 = NULL;
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "GHashTable"),
      ==, 0);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 1);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "MyObject"),
      ==, 1);
  my_object_free (fixture->mo); fixture->mo = NULL;
  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, "MyObject"),
      ==, 0);

  g_assert_cmpuint (gum_cobject_tracker_peek_total_count (t, NULL), ==, 0);
}

TESTCASE (object_list)
{
  GList * cobjects, * cur;

  test_cobject_tracker_fixture_enable_backtracer (fixture);

  fixture->ht1 = g_hash_table_new_full (NULL, NULL, NULL, NULL);
  fixture->mo = my_object_new ();

  cobjects = gum_cobject_tracker_peek_object_list (fixture->tracker);
  g_assert_cmpint (g_list_length (cobjects), ==, 2);

  for (cur = cobjects; cur != NULL; cur = cur->next)
  {
    GumCObject * cobject = (GumCObject *) cur->data;

    g_assert_true (cobject->address == fixture->ht1 ||
        cobject->address == fixture->mo);

    if (cobject->address == fixture->ht1)
      g_assert_cmpstr (cobject->type_name, ==, "GHashTable");
    else
      g_assert_cmpstr (cobject->type_name, ==, "MyObject");

    {
#ifdef HAVE_WINDOWS
      GumReturnAddressDetails rad;

      g_assert_true (gum_return_address_details_from_address (
          cobject->return_addresses.items[0], &rad));
      g_assert_cmpstr (rad.function_name, ==, __FUNCTION__);
      g_assert_cmpint (rad.line_number, >, 0);
#else
      g_assert_nonnull (cobject->return_addresses.items[0]);
#endif
    }
  }

  gum_cobject_list_free (cobjects);
}

#endif /* HAVE_WINDOWS */
