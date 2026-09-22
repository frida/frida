/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemoryaccessmonitor.h"

/*
 * The implementation lives in the per-platform backends
 * (backend-posix/, backend-windows/, backend-barebone/). This file carries the
 * shared, platform-independent API documentation.
 */

/**
 * GumMemoryAccessMonitor:
 *
 * Monitors a set of memory ranges and reports accesses to them.
 *
 * The monitor strips the watched permissions from the pages backing each range
 * and uses a [class@Gum.Exceptor] to catch the resulting access violations, so
 * it needs no debugger. Whenever monitored memory is touched in a way that
 * matches the access mask, a [callback@Gum.MemoryAccessNotify] fires with the
 * details of the access.
 *
 * A common use is coverage-style tracking: with auto-reset enabled each page is
 * reported the first time it is touched and then runs unhindered, while the
 * `pages_completed` / `pages_total` fields of [struct@Gum.MemoryAccessDetails]
 * reveal how much of the watched memory has been reached.
 *
 * ```c
 * static void on_access (GumMemoryAccessMonitor * monitor,
 *     const GumMemoryAccessDetails * details, gpointer user_data);
 *
 * void
 * watch (gpointer base, gsize size)
 * {
 *   GumMemoryRange range = { GUM_ADDRESS (base), size };
 *   g_autoptr(GError) error = NULL;
 *   GumMemoryAccessMonitor * monitor = gum_memory_access_monitor_new (
 *       &range, 1, GUM_PAGE_WRITE, TRUE, on_access, NULL, NULL);
 *
 *   if (!gum_memory_access_monitor_enable (monitor, &error))
 *     g_printerr ("Failed to enable: %s\n", error->message);
 * }
 * ```
 */

/**
 * GumMemoryAccessNotify:
 * @monitor: the #GumMemoryAccessMonitor reporting the access
 * @details: details of the access
 * @user_data: the data passed to [ctor@Gum.MemoryAccessMonitor.new]
 *
 * The type of function notified when monitored memory is accessed.
 */

/**
 * GumMemoryAccessDetails:
 * @thread_id: ID of the thread that performed the access
 * @operation: the kind of access — read, write or execute
 * @from: address of the instruction that made the access
 * @address: the memory address that was accessed
 * @range_index: index of the monitored range the address falls in
 * @page_index: index of the page within that range
 * @pages_completed: number of monitored pages accessed so far
 * @pages_total: total number of monitored pages
 * @context: CPU context at the point of the access
 *
 * Describes a single access to monitored memory, as delivered to a
 * [callback@Gum.MemoryAccessNotify].
 */

/**
 * gum_memory_access_monitor_new:
 * @ranges: (array length=num_ranges): the memory ranges to monitor
 * @num_ranges: the number of ranges
 * @access_mask: which access types to watch, as a mask of #GumPageProtection
 *   bits (e.g. %GUM_PAGE_WRITE to catch writes); these permissions are removed
 *   from the pages so matching accesses fault and get reported
 * @auto_reset: whether to restore a page and stop watching it after its first
 *   matching access, instead of reporting every access
 * @func: (scope notified): function to call on each reported access
 * @data: data to pass to @func
 * @data_destroy: (nullable): destroy notify for @data
 *
 * Creates a monitor for the given ranges, each rounded out to page granularity.
 * Call [method@Gum.MemoryAccessMonitor.enable] to start watching.
 *
 * Returns: (transfer full): a new #GumMemoryAccessMonitor
 */

/**
 * gum_memory_access_monitor_enable:
 * @self: a #GumMemoryAccessMonitor
 * @error: return location for a #GError
 *
 * Starts watching the configured ranges, stripping the masked permissions from
 * their pages and installing the exception handler. Fails if a monitored page
 * is unallocated or already fully inaccessible.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error set
 */

/**
 * gum_memory_access_monitor_disable:
 * @self: a #GumMemoryAccessMonitor
 *
 * Stops watching, restoring the original page permissions and removing the
 * exception handler. Safe to call when not enabled.
 */
