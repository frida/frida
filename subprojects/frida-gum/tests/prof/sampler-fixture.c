/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsampler.h"

#include "gumusertimesampler.h"
#include "testutil.h"
#include "valgrind.h"

#include <stdlib.h>

#define TESTCASE(NAME) \
    void test_sampler_ ## NAME ( \
        TestSamplerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Prof/Sampler", test_sampler, NAME, \
        TestSamplerFixture)

typedef struct _TestSamplerFixture
{
  GumSampler * sampler;
} TestSamplerFixture;

static void
test_sampler_fixture_setup (TestSamplerFixture * fixture,
                            gconstpointer data)
{
}

static void
test_sampler_fixture_teardown (TestSamplerFixture * fixture,
                               gconstpointer data)
{
  g_clear_object (&fixture->sampler);
}
