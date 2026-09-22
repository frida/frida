/*
 * Copyright (C) 2018-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumjs.h"

#include "gumscriptbackend.h"

void
gumjs_prepare_to_fork (void)
{
  gum_script_scheduler_stop (gum_script_backend_get_scheduler ());
}

void
gumjs_recover_from_fork_in_parent (void)
{
  gum_script_scheduler_start (gum_script_backend_get_scheduler ());
}

void
gumjs_recover_from_fork_in_child (void)
{
  gum_script_scheduler_start (gum_script_backend_get_scheduler ());
}
