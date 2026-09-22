/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2026 Ricardo Marques <marquessricardo@gmail.com>
 * Copyright (C) 2026 IPMegladon <ipmegladon@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "testutil.h"

#include "gummemory-priv.h"

#define TESTCASE(NAME) \
    void test_memory_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/Memory", test_memory, NAME)

TESTLIST_BEGIN (memory)
  TESTENTRY (read_from_valid_address_should_succeed)
  TESTENTRY (read_from_invalid_address_should_fail)
  TESTENTRY (read_from_unaligned_address_should_succeed)
  TESTENTRY (read_across_two_pages_should_return_correct_data)
  TESTENTRY (read_beyond_page_should_return_partial_data)
  TESTENTRY (write_to_valid_address_should_succeed)
  TESTENTRY (write_to_invalid_address_should_fail)
  TESTENTRY (match_pattern_from_string_does_proper_validation)
  TESTENTRY (scan_range_finds_three_exact_matches)
  TESTENTRY (scan_range_finds_three_wildcarded_matches)
  TESTENTRY (scan_range_finds_three_masked_matches)
  TESTENTRY (scan_range_finds_three_regex_matches)
  TESTENTRY (scan_range_ignores_match_starting_before_range)
  TESTENTRY (scan_range_skips_overlapping_matches)
  TESTENTRY (scan_range_finds_three_leading_wildcard_matches)
  TESTENTRY (scan_range_finds_three_trailing_wildcard_matches)
  TESTENTRY (find_pointers_finds_exact_value)
  TESTENTRY (find_pointers_finds_multiple_values)
  TESTENTRY (find_pointers_finds_many_values)
#if GLIB_SIZEOF_VOID_P == 8
  TESTENTRY (find_pointers_rejects_partial_word_match)
#endif
  TESTENTRY (find_pointers_applies_mask)
  TESTENTRY (find_pointers_returns_sorted_matches_across_tiles)
  TESTENTRY (find_pointers_returns_empty_array_when_absent)
  TESTENTRY (find_pointers_skips_inaccessible_range)
  TESTENTRY (find_pointers_skips_inaccessible_range_in_parallel_scan)
  TESTENTRY (is_memory_readable_handles_mixed_page_protections)
  TESTENTRY (alloc_n_pages_returns_aligned_rw_address)
  TESTENTRY (alloc_n_pages_near_returns_aligned_rw_address_within_range)
  TESTENTRY (allocate_handles_alignment)
  TESTENTRY (allocate_near_handles_alignment)
  TESTENTRY (mprotect_handles_page_boundaries)
  TESTENTRY (patch_code_does_not_apply_while_threads_suspended)
TESTLIST_END ()

typedef struct _TestForEachContext {
  gboolean value_to_return;
  guint number_of_calls;

  gpointer expected_address[3];
  guint expected_size;
} TestForEachContext;

static gboolean match_found_cb (GumAddress address, gsize size,
    gpointer user_data);
static gpointer gum_patch_lock_holder (gpointer data);
static void gum_patch_gated_apply (gpointer mem, gpointer user_data);

static GMutex gum_patch_gate;
static volatile gint gum_patch_holder_ready;
static volatile gint gum_patch_holder_release;

TESTCASE (read_from_valid_address_should_succeed)
{
  guint8 magic[2] = { 0x13, 0x37 };
  gsize n_bytes_read;
  guint8 * result;

  result = gum_memory_read (magic, sizeof (magic), &n_bytes_read);
  g_assert_nonnull (result);

  g_assert_cmpuint (n_bytes_read, ==, sizeof (magic));

  g_assert_cmphex (result[0], ==, magic[0]);
  g_assert_cmphex (result[1], ==, magic[1]);

  g_free (result);
}

TESTCASE (read_from_invalid_address_should_fail)
{
  guint8 * invalid_address = GSIZE_TO_POINTER (0x42);
  g_assert_null (gum_memory_read (invalid_address, 1, NULL));
}

TESTCASE (read_from_unaligned_address_should_succeed)
{
  gpointer page;
  guint page_size;
  guint8 * last_byte;
  gsize n_bytes_read;
  guint8 * data;

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  last_byte = ((guint8 *) page) + page_size - 1;
  *last_byte = 42;
  data = gum_memory_read (last_byte, 1, &n_bytes_read);
  g_assert_nonnull (data);
  g_assert_cmpuint (n_bytes_read, ==, 1);
  g_assert_cmpuint (*data, ==, 42);
  g_free (data);

  gum_memory_free (page, page_size);
}

TESTCASE (read_across_two_pages_should_return_correct_data)
{
  GRand * rand;
  gsize page_size;
  guint8 * pages;
  guint size, i, start_offset;
  gchar * expected_checksum, * actual_checksum;
  guint8 * data;
  gsize n_bytes_read;

  rand = g_rand_new_with_seed (42);
  page_size = gum_query_page_size ();
  pages = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);
  size = 2 * page_size;
  start_offset = (size / 2) - 1;
  for (i = start_offset; i != size; i++)
  {
    pages[i] = (guint8) g_rand_int_range (rand, 0, 255);
  }
  expected_checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
      pages + start_offset, size - start_offset);

  data = gum_memory_read (pages + start_offset, size - start_offset,
      &n_bytes_read);
  g_assert_nonnull (data);
  g_assert_cmpuint (n_bytes_read, ==, size - start_offset);
  actual_checksum =
      g_compute_checksum_for_data (G_CHECKSUM_SHA1, data, n_bytes_read);
  g_assert_cmpstr (actual_checksum, ==, expected_checksum);
  g_free (actual_checksum);
  g_free (data);

  g_free (expected_checksum);
  gum_memory_free (pages, 2 * page_size);
  g_rand_free (rand);
}

TESTCASE (read_beyond_page_should_return_partial_data)
{
  guint8 * page;
  guint page_size;
  gsize n_bytes_read;
  guint8 * data;

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);
  gum_mprotect (page + page_size, page_size, GUM_PAGE_NO_ACCESS);

  data = gum_memory_read (page, 2 * page_size, &n_bytes_read);
  g_assert_nonnull (data);
  g_assert_cmpuint (n_bytes_read, ==, page_size);
  g_free (data);

  data = gum_memory_read (page + page_size - 1, 1 + page_size, &n_bytes_read);
  g_assert_nonnull (data);
  g_assert_cmpuint (n_bytes_read, ==, 1);
  g_free (data);

  gum_memory_free (page, 2 * page_size);
}

TESTCASE (write_to_valid_address_should_succeed)
{
  guint8 bytes[3] = { 0x00, 0x00, 0x12 };
  guint8 magic[2] = { 0x13, 0x37 };

  g_assert_true (gum_memory_write (bytes, magic, sizeof (magic)));

  g_assert_cmphex (bytes[0], ==, 0x13);
  g_assert_cmphex (bytes[1], ==, 0x37);
  g_assert_cmphex (bytes[2], ==, 0x12);
}

TESTCASE (write_to_invalid_address_should_fail)
{
  guint8 bytes[3] = { 0x00, 0x00, 0x12 };
  guint8 * invalid_address = GSIZE_TO_POINTER (0x42);
  g_assert_false (gum_memory_write (invalid_address, bytes, sizeof (bytes)));
}

#define GUM_PATTERN_NTH_TOKEN(p, n) \
    ((GumMatchToken *) g_ptr_array_index (gum_match_pattern_get_tokens (p), n))
#define GUM_PATTERN_NTH_TOKEN_NTH_BYTE(p, n, b) \
    (g_array_index (((GumMatchToken *) g_ptr_array_index ( \
        gum_match_pattern_get_tokens (p), n))->bytes, guint8, b))
#define GUM_PATTERN_NTH_TOKEN_NTH_MASK(p, n, b) \
    (g_array_index (((GumMatchToken *) g_ptr_array_index ( \
        gum_match_pattern_get_tokens (p), n))->masks, guint8, b))

TESTCASE (match_pattern_from_string_does_proper_validation)
{
  GumMatchPattern * pattern;

  pattern = gum_match_pattern_new_from_string ("1337");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 2);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 1), ==, 0x37);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 37");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 2);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 1), ==, 0x37);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("1 37");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("13 3");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("13+37");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("13 ?? 37");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 3);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 3);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 1)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 1, 0), ==, 0x42);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 2)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 2, 0), ==, 0x37);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 ? 37");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("??");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("?? 13");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 2);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 ??");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 2);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("?? ?? ??");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string (" ");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("");
  g_assert_null (pattern);

  pattern = gum_match_pattern_new_from_string ("1337:ff0f");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 2);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 1)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 1, 0), ==, 0x37);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_MASK (pattern, 1, 0), ==, 0x0f);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 37 : ff 0f");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 2);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 1)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 1, 0), ==, 0x37);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_MASK (pattern, 1, 0), ==, 0x0f);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 ?7");
  g_assert_nonnull (pattern);
  g_assert_cmpuint (gum_match_pattern_get_size (pattern), ==, 2);
  g_assert_cmpuint (gum_match_pattern_get_tokens (pattern)->len, ==, 2);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 1)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 1, 0), ==, 0x47);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_MASK (pattern, 1, 0), ==, 0x0f);
  gum_match_pattern_unref (pattern);

  pattern = gum_match_pattern_new_from_string ("13 37 : ff");
  g_assert_null (pattern);
}

TESTCASE (scan_range_finds_three_exact_matches)
{
  guint8 buf[] = {
    0x13, 0x37,
    0x12,
    0x13, 0x37,
    0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("13 37");
  g_assert_nonnull (pattern);

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 2 + 1;
  ctx.expected_address[2] = buf + 2 + 1 + 2;
  ctx.expected_size = 2;

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_memory_scan (&range, pattern, match_found_cb, &ctx);
  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  ctx.number_of_calls = 0;
  ctx.value_to_return = FALSE;
  gum_memory_scan (&range, pattern, match_found_cb, &ctx);
  g_assert_cmpuint (ctx.number_of_calls, ==, 1);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_finds_three_wildcarded_matches)
{
  guint8 buf[] = {
    0x12, 0x11, 0x13, 0x37,
    0x12, 0x00,
    0x12, 0xc0, 0x13, 0x37,
    0x12, 0x44, 0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("12 ?? 13 37");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 4 + 2;
  ctx.expected_address[2] = buf + 4 + 2 + 4;
  ctx.expected_size = 4;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_finds_three_masked_matches)
{
  guint8 buf[] = {
    0x12, 0x11, 0x13, 0x35,
    0x12, 0x00,
    0x72, 0xc0, 0x13, 0x37,
    0xb2, 0x44, 0x13, 0x33
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("12 ?? 13 37 : 1f ff ff f1");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 4 + 2;
  ctx.expected_address[2] = buf + 4 + 2 + 4;
  ctx.expected_size = 4;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_finds_three_regex_matches)
{
  gchar buf[] = "Brainfuck_OR_brainsuckANDbrainluck\nbrainmuck";
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("/[Bb]rain[fsm]..k/");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + sizeof ("Brainfuck_OR_") - 1;
  ctx.expected_address[2] = buf +
      sizeof ("Brainfuck_OR_brainsuckANDbrainluck\n") - 1;
  ctx.expected_size = 9;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_ignores_match_starting_before_range)
{
  guint8 buf[] = {
    0x13, 0x00, 0x37, 0x37,
    0x00, 0x00, 0x00, 0x00
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf + 2);
  range.size = 2;

  pattern = gum_match_pattern_new_from_string ("13 ?? 37 37");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 0);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_skips_overlapping_matches)
{
  guint8 buf[] = {
    0x13, 0x00, 0x13, 0x37,
    0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("13 ?? 13 37");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_size = 4;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 1);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_finds_three_leading_wildcard_matches)
{
  guint8 buf[] = {
    0xaa, 0x13, 0x37,
    0x12,
    0xbb, 0x13, 0x37,
    0xcc, 0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("?? 13 37");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 4;
  ctx.expected_address[2] = buf + 7;
  ctx.expected_size = 3;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_unref (pattern);
}

TESTCASE (scan_range_finds_three_trailing_wildcard_matches)
{
  guint8 buf[] = {
    0x13, 0x37, 0xaa,
    0x12,
    0x13, 0x37, 0xbb,
    0x13, 0x37, 0xcc
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("13 37 ??");
  g_assert_nonnull (pattern);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 4;
  ctx.expected_address[2] = buf + 7;
  ctx.expected_size = 3;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_unref (pattern);
}

TESTCASE (find_pointers_finds_exact_value)
{
  gsize buf[] = { 0x1111, 0xdeadbeef, 0x2222, 0xdeadbeef };
  GumMemoryRange range;
  gsize value;
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  value = 0xdeadbeef;
  matches = gum_memory_find_pointers (&range, 1, &value, 1, G_MAXSIZE);

  g_assert_cmpuint (matches->len, ==, 2);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 0).address, ==,
      GUM_ADDRESS (&buf[1]));
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 0).value, ==,
      0xdeadbeef);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 1).address, ==,
      GUM_ADDRESS (&buf[3]));

  g_array_free (matches, TRUE);
}

TESTCASE (find_pointers_finds_multiple_values)
{
  gsize buf[] = { 0xaaaa, 0xbbbb, 0xcccc, 0xbbbb };
  GumMemoryRange range;
  gsize values[] = { 0xaaaa, 0xbbbb };
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  matches = gum_memory_find_pointers (&range, 1, values, G_N_ELEMENTS (values),
      G_MAXSIZE);

  g_assert_cmpuint (matches->len, ==, 3);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 0).value, ==,
      0xaaaa);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 1).value, ==,
      0xbbbb);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 2).value, ==,
      0xbbbb);

  g_array_free (matches, TRUE);
}

TESTCASE (find_pointers_finds_many_values)
{
  gsize buf[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70 };
  GumMemoryRange range;
  gsize values[] = { 0x20, 0x40, 0x60, 0x70, 0x999 };
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  matches = gum_memory_find_pointers (&range, 1, values, G_N_ELEMENTS (values),
      G_MAXSIZE);

  g_assert_cmpuint (matches->len, ==, 4);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 0).value, ==, 0x20);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 1).value, ==, 0x40);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 2).value, ==, 0x60);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 3).value, ==, 0x70);

  g_array_free (matches, TRUE);
}

#if GLIB_SIZEOF_VOID_P == 8

TESTCASE (find_pointers_rejects_partial_word_match)
{
  gsize buf[] = { 0x2222222211111111 };
  GumMemoryRange range;
  gsize values[] = { 0xaaaaaaaa11111111, 0x22222222bbbbbbbb };
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  matches = gum_memory_find_pointers (&range, 1, values, G_N_ELEMENTS (values),
      G_MAXSIZE);

  g_assert_cmpuint (matches->len, ==, 0);

  g_array_free (matches, TRUE);
}

#endif

TESTCASE (find_pointers_applies_mask)
{
  gsize tag = (gsize) 0xff << (8 * (sizeof (gpointer) - 1));
  gsize buf[] = { tag | 0x1000, 0x1000 };
  GumMemoryRange range;
  gsize value, mask;
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  value = 0x1000;
  mask = ~tag;
  matches = gum_memory_find_pointers (&range, 1, &value, 1, mask);

  g_assert_cmpuint (matches->len, ==, 2);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 0).value, ==,
      tag | 0x1000);
  g_assert_cmphex (g_array_index (matches, GumPointerMatch, 1).value, ==,
      0x1000);

  g_array_free (matches, TRUE);
}

TESTCASE (find_pointers_returns_sorted_matches_across_tiles)
{
  guint n_words = 4 * 1024 * 1024;
  gsize * buf;
  GumMemoryRange range;
  gsize value;
  GArray * matches;
  guint i;

  buf = g_new0 (gsize, n_words);
  value = 0x1337;
  buf[7] = value;
  buf[n_words / 2] = value;
  buf[n_words - 3] = value;

  range.base_address = GUM_ADDRESS (buf);
  range.size = n_words * sizeof (gsize);

  matches = gum_memory_find_pointers (&range, 1, &value, 1, G_MAXSIZE);

  g_assert_cmpuint (matches->len, ==, 3);
  for (i = 1; i != matches->len; i++)
  {
    g_assert_cmphex (g_array_index (matches, GumPointerMatch, i - 1).address,
        <, g_array_index (matches, GumPointerMatch, i).address);
  }

  g_array_free (matches, TRUE);
  g_free (buf);
}

TESTCASE (find_pointers_returns_empty_array_when_absent)
{
  gsize buf[] = { 0x1, 0x2, 0x3 };
  GumMemoryRange range;
  gsize value;
  GArray * matches;

  range.base_address = GUM_ADDRESS (buf);
  range.size = sizeof (buf);

  value = 0xabcd;
  matches = gum_memory_find_pointers (&range, 1, &value, 1, G_MAXSIZE);

  g_assert_nonnull (matches);
  g_assert_cmpuint (matches->len, ==, 0);

  g_array_free (matches, TRUE);
}

TESTCASE (find_pointers_skips_inaccessible_range)
{
  gsize before[] = { 0x1337 };
  gsize after[] = { 0x1337 };
  gsize value = 0x1337;
  guint page_size;
  guint8 * inaccessible;
  GumMemoryRange ranges[3];
  GArray * matches;
  gboolean found_before = FALSE;
  gboolean found_after = FALSE;
  guint i;

  page_size = gum_query_page_size ();
  inaccessible = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_mprotect (inaccessible, page_size, GUM_PAGE_NO_ACCESS);

  ranges[0].base_address = GUM_ADDRESS (before);
  ranges[0].size = sizeof (before);
  ranges[1].base_address = GUM_ADDRESS (inaccessible);
  ranges[1].size = page_size;
  ranges[2].base_address = GUM_ADDRESS (after);
  ranges[2].size = sizeof (after);

  matches = gum_memory_find_pointers (ranges, G_N_ELEMENTS (ranges), &value,
      1, G_MAXSIZE);

  for (i = 0; i != matches->len; i++)
  {
    GumPointerMatch match = g_array_index (matches, GumPointerMatch, i);

    if (match.address == GUM_ADDRESS (before))
      found_before = TRUE;
    if (match.address == GUM_ADDRESS (after))
      found_after = TRUE;
  }

  g_assert_cmpuint (matches->len, ==, 2);
  g_assert_true (found_before);
  g_assert_true (found_after);

  g_array_free (matches, TRUE);
  gum_memory_free (inaccessible, page_size);
}

TESTCASE (find_pointers_skips_inaccessible_range_in_parallel_scan)
{
  gsize size = 4 * 1024 * 1024;
  gsize trailing[] = { 0x1337 };
  gsize value = 0x1337;
  gsize * large;
  guint page_size;
  guint8 * inaccessible;
  GumMemoryRange ranges[3];
  GArray * matches;
  gboolean found_large = FALSE;
  gboolean found_trailing = FALSE;
  guint i;

  large = g_new0 (gsize, size / sizeof (gsize));
  large[0] = value;

  page_size = gum_query_page_size ();
  inaccessible = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_mprotect (inaccessible, page_size, GUM_PAGE_NO_ACCESS);

  ranges[0].base_address = GUM_ADDRESS (large);
  ranges[0].size = size;
  ranges[1].base_address = GUM_ADDRESS (inaccessible);
  ranges[1].size = page_size;
  ranges[2].base_address = GUM_ADDRESS (trailing);
  ranges[2].size = sizeof (trailing);

  matches = gum_memory_find_pointers (ranges, G_N_ELEMENTS (ranges), &value,
      1, G_MAXSIZE);

  for (i = 0; i != matches->len; i++)
  {
    GumPointerMatch match = g_array_index (matches, GumPointerMatch, i);

    if (match.address == GUM_ADDRESS (large))
      found_large = TRUE;
    if (match.address == GUM_ADDRESS (trailing))
      found_trailing = TRUE;
  }

  g_assert_cmpuint (matches->len, ==, 2);
  g_assert_true (found_large);
  g_assert_true (found_trailing);

  g_array_free (matches, TRUE);
  gum_memory_free (inaccessible, page_size);
  g_free (large);
}

TESTCASE (is_memory_readable_handles_mixed_page_protections)
{
  guint8 * pages;
  guint page_size;
  guint8 * left_guard, * first_page, * second_page, * right_guard;

  page_size = gum_query_page_size ();

  pages = gum_memory_allocate (NULL, 4 * page_size, page_size, GUM_PAGE_RW);

  left_guard = pages;
  first_page = left_guard + page_size;
  second_page = first_page + page_size;
  right_guard = second_page + page_size;

  gum_mprotect (left_guard, page_size, GUM_PAGE_NO_ACCESS);
  gum_mprotect (right_guard, page_size, GUM_PAGE_NO_ACCESS);

  g_assert_true (gum_memory_is_readable (first_page, 1));
  g_assert_true (gum_memory_is_readable (first_page + page_size - 1, 1));
  g_assert_true (gum_memory_is_readable (first_page, page_size));

  g_assert_true (gum_memory_is_readable (second_page, 1));
  g_assert_true (gum_memory_is_readable (second_page + page_size - 1, 1));
  g_assert_true (gum_memory_is_readable (second_page, page_size));

  g_assert_true (gum_memory_is_readable (first_page + page_size - 1, 2));
  g_assert_true (gum_memory_is_readable (first_page, 2 * page_size));

  g_assert_false (gum_memory_is_readable (second_page + page_size, 1));
  g_assert_false (gum_memory_is_readable (second_page + page_size - 1, 2));

  gum_memory_free (pages, 4 * page_size);
}

TESTCASE (alloc_n_pages_returns_aligned_rw_address)
{
  gpointer page;
  guint page_size;

  page_size = gum_query_page_size ();

  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  g_assert_cmpuint (GPOINTER_TO_SIZE (page) % page_size, ==, 0);

  g_assert_true (gum_memory_is_readable (page, page_size));

  g_assert_cmpuint (*((gsize *) page), ==, 0);
  *((gsize *) page) = 42;
  g_assert_cmpuint (*((gsize *) page), ==, 42);

  gum_memory_free (page, page_size);
}

TESTCASE (alloc_n_pages_near_returns_aligned_rw_address_within_range)
{
  GumAddressSpec as;
  guint variable_on_stack;
  guint page_size;
  gpointer page;
  gsize actual_distance;

  as.near_address = &variable_on_stack;
  as.max_distance = G_MAXINT32;

  page_size = gum_query_page_size ();
  page = gum_memory_allocate_near (&as, page_size, page_size, GUM_PAGE_RW);
  if (page == NULL)
  {
    g_print ("<skipping, not supported on this system> ");
    return;
  }

  g_assert_cmpuint (GPOINTER_TO_SIZE (page) % page_size, ==, 0);

  g_assert_true (gum_memory_is_readable (page, page_size));

  g_assert_cmpuint (*((gsize *) page), ==, 0);
  *((gsize *) page) = 42;
  g_assert_cmpuint (*((gsize *) page), ==, 42);

  actual_distance = ABS ((guint8 *) page - (guint8 *) as.near_address);
  g_assert_cmpuint (actual_distance, <=, as.max_distance);

  gum_memory_free (page, page_size);
}

TESTCASE (allocate_handles_alignment)
{
  gsize size, alignment;
  gpointer page;

  size = gum_query_page_size ();
  alignment = 1024 * 1024;

  page = gum_memory_allocate (NULL, size, alignment, GUM_PAGE_RW);
  g_assert_nonnull (page);
  g_assert_cmpuint (GPOINTER_TO_SIZE (page) % alignment, ==, 0);

  gum_memory_free (page, size);
}

TESTCASE (allocate_near_handles_alignment)
{
  GumAddressSpec as;
  guint variable_on_stack;
  gsize size, alignment;
  gpointer page;

#if defined (HAVE_FREEBSD) && defined (HAVE_ARM64)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  as.near_address = &variable_on_stack;
  as.max_distance = G_MAXINT32;

  size = gum_query_page_size ();
  alignment = 1024 * 1024;

  page = gum_memory_allocate_near (&as, size, alignment, GUM_PAGE_RW);
  g_assert_nonnull (page);
  g_assert_cmpuint (GPOINTER_TO_SIZE (page) % alignment, ==, 0);

  gum_memory_free (page, size);
}

TESTCASE (mprotect_handles_page_boundaries)
{
  guint8 * pages;
  guint page_size;

  page_size = gum_query_page_size ();
  pages = gum_memory_allocate (NULL, 2 * page_size, page_size,
      GUM_PAGE_NO_ACCESS);
  gum_memory_recommit (pages, 2 * page_size, GUM_PAGE_NO_ACCESS);

  gum_mprotect (pages + page_size - 1, 2, GUM_PAGE_RW);
  pages[page_size - 1] = 0x13;
  pages[page_size] = 0x37;

  gum_memory_free (pages, 2 * page_size);
}

static gboolean
match_found_cb (GumAddress address,
                gsize size,
                gpointer user_data)
{
  TestForEachContext * ctx = (TestForEachContext *) user_data;

  g_assert_cmpuint (ctx->number_of_calls, <, 3);

  g_assert_cmpuint (address, ==,
      GUM_ADDRESS (ctx->expected_address[ctx->number_of_calls]));
  g_assert_cmpuint (size, ==, ctx->expected_size);

  ctx->number_of_calls++;

  return ctx->value_to_return;
}

TESTCASE (patch_code_does_not_apply_while_threads_suspended)
{
  GThread * holder;
  gsize page_size;
  guint8 * code;

  g_mutex_init (&gum_patch_gate);
  g_atomic_int_set (&gum_patch_holder_ready, 0);
  g_atomic_int_set (&gum_patch_holder_release, 0);

  holder = g_thread_new ("patch-lock-holder", gum_patch_lock_holder, NULL);
  while (g_atomic_int_get (&gum_patch_holder_ready) == 0)
    g_thread_yield ();

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  code[0] = 0x00;
  gum_mprotect (code, page_size, GUM_PAGE_RX);

  gum_memory_patch_code (code, 1, gum_patch_gated_apply, NULL);

  g_assert_cmphex (code[0], ==, 0xc3);

  g_thread_join (holder);

  gum_memory_free (code, page_size);
  g_mutex_clear (&gum_patch_gate);
}

static gpointer
gum_patch_lock_holder (gpointer data)
{
  g_mutex_lock (&gum_patch_gate);
  g_atomic_int_set (&gum_patch_holder_ready, 1);
  while (g_atomic_int_get (&gum_patch_holder_release) == 0)
    g_thread_yield ();
  g_mutex_unlock (&gum_patch_gate);

  return NULL;
}

static void
gum_patch_gated_apply (gpointer mem,
                       gpointer user_data)
{
  *((guint8 *) mem) = 0xc3;

  g_atomic_int_set (&gum_patch_holder_release, 1);

  g_mutex_lock (&gum_patch_gate);
  g_mutex_unlock (&gum_patch_gate);
}
