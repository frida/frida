/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2022-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_H__
#define __GUM_DARWIN_H__

#include "gumdarwinmapper.h"
#include "gumdarwinmodule.h"
#include "gumdarwinmoduleresolver.h"
#include "gumdarwinsymbolicator.h"
#include "gummemory.h"
#include "gumprocess.h"

#include <TargetConditionals.h>
#include <mach/mach.h>
#include <os/lock.h>
#include <sys/param.h>
#include <sys/queue.h>
#if TARGET_OS_OSX
# include <mach/mach_vm.h>
#else
kern_return_t mach_vm_allocate (vm_map_t target, mach_vm_address_t * address,
    mach_vm_size_t size, int flags);
kern_return_t mach_vm_deallocate (vm_map_t target, mach_vm_address_t address,
    mach_vm_size_t size);
kern_return_t mach_vm_protect (vm_map_t target_task, mach_vm_address_t address,
    mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);
kern_return_t mach_vm_inherit (vm_map_t target_task, mach_vm_address_t address,
    mach_vm_size_t size, vm_inherit_t new_inheritance);
kern_return_t mach_vm_read (vm_map_t target_task, mach_vm_address_t address,
    mach_vm_size_t size, vm_offset_t * data, mach_msg_type_number_t * data_cnt);
kern_return_t mach_vm_read_list (vm_map_t target_task,
    mach_vm_read_entry_t data_list, natural_t count);
kern_return_t mach_vm_write (vm_map_t target_task, mach_vm_address_t address,
    vm_offset_t data, mach_msg_type_number_t data_cnt);
kern_return_t mach_vm_copy (vm_map_t target_task,
    mach_vm_address_t source_address, mach_vm_size_t size,
    mach_vm_address_t dest_address);
kern_return_t mach_vm_read_overwrite (vm_map_t target_task,
    mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data,
    mach_vm_size_t * outsize);
kern_return_t mach_vm_msync (vm_map_t target_task, mach_vm_address_t address,
    mach_vm_size_t size, vm_sync_t sync_flags);
kern_return_t mach_vm_behavior_set (vm_map_t target_task,
    mach_vm_address_t address, mach_vm_size_t size, vm_behavior_t new_behavior);
kern_return_t mach_vm_map (vm_map_t target_task, mach_vm_address_t * address,
    mach_vm_size_t size, mach_vm_offset_t mask, int flags,
    mem_entry_name_port_t object, memory_object_offset_t offset, boolean_t copy,
    vm_prot_t cur_protection, vm_prot_t max_protection,
    vm_inherit_t inheritance);
kern_return_t mach_vm_machine_attribute (vm_map_t target_task,
    mach_vm_address_t address, mach_vm_size_t size,
    vm_machine_attribute_t attribute, vm_machine_attribute_val_t * value);
kern_return_t mach_vm_remap (vm_map_t target_task,
    mach_vm_address_t * target_address, mach_vm_size_t size,
    mach_vm_offset_t mask, int flags, vm_map_t src_task,
    mach_vm_address_t src_address, boolean_t copy, vm_prot_t * cur_protection,
    vm_prot_t * max_protection, vm_inherit_t inheritance);
kern_return_t mach_vm_page_query (vm_map_t target_map, mach_vm_offset_t offset,
    integer_t * disposition, integer_t * ref_count);
kern_return_t mach_vm_region_recurse (vm_map_t target_task,
    mach_vm_address_t * address, mach_vm_size_t * size,
    natural_t * nesting_depth, vm_region_recurse_info_t info,
    mach_msg_type_number_t * info_cnt);
kern_return_t mach_vm_region (vm_map_t target_task, mach_vm_address_t * address,
    mach_vm_size_t * size, vm_region_flavor_t flavor, vm_region_info_t info,
    mach_msg_type_number_t * info_cnt, mach_port_t * object_name);
kern_return_t _mach_make_memory_entry (vm_map_t target_task,
    memory_object_size_t * size, memory_object_offset_t offset,
    vm_prot_t permission, mem_entry_name_port_t * object_handle,
    mem_entry_name_port_t parent_handle);
kern_return_t mach_vm_purgable_control (vm_map_t target_task,
    mach_vm_address_t address, vm_purgable_t control, int * state);
kern_return_t mach_vm_page_info (vm_map_t target_task,
    mach_vm_address_t address, vm_page_info_flavor_t flavor,
    vm_page_info_t info, mach_msg_type_number_t * info_cnt);
kern_return_t mach_vm_page_range_query (vm_map_t target_map,
    mach_vm_offset_t address, mach_vm_size_t size,
    mach_vm_address_t dispositions, mach_vm_size_t * dispositions_count);
#endif

G_BEGIN_DECLS

#if GLIB_SIZEOF_VOID_P == 4
# define GUM_LC_SEGMENT LC_SEGMENT
typedef struct mach_header gum_mach_header_t;
typedef struct segment_command gum_segment_command_t;
typedef struct section gum_section_t;
typedef struct nlist gum_nlist_t;
#else
# define GUM_LC_SEGMENT LC_SEGMENT_64
typedef struct mach_header_64 gum_mach_header_t;
typedef struct segment_command_64 gum_segment_command_t;
typedef struct section_64 gum_section_t;
typedef struct nlist_64 gum_nlist_t;
#endif

#if !defined (__arm__) && !defined (__aarch64__)
typedef x86_thread_state_t GumDarwinUnifiedThreadState;
# if GLIB_SIZEOF_VOID_P == 4
typedef x86_thread_state32_t GumDarwinNativeThreadState;
typedef x86_float_state32_t GumDarwinNativeFloatState;
typedef x86_debug_state32_t GumDarwinNativeDebugState;
#  define GUM_DARWIN_FLOAT_STATE_COUNT x86_FLOAT_STATE32_COUNT
#  define GUM_DARWIN_FLOAT_STATE_FLAVOR x86_FLOAT_STATE32
# else
typedef x86_thread_state64_t GumDarwinNativeThreadState;
typedef x86_float_state64_t GumDarwinNativeFloatState;
typedef x86_debug_state64_t GumDarwinNativeDebugState;
#  define GUM_DARWIN_FLOAT_STATE_COUNT x86_FLOAT_STATE64_COUNT
#  define GUM_DARWIN_FLOAT_STATE_FLAVOR x86_FLOAT_STATE64
# endif
# define GUM_DARWIN_THREAD_STATE_COUNT x86_THREAD_STATE_COUNT
# define GUM_DARWIN_THREAD_STATE_FLAVOR x86_THREAD_STATE
# define GUM_DARWIN_DEBUG_STATE_COUNT x86_DEBUG_STATE64_COUNT
# define GUM_DARWIN_DEBUG_STATE_FLAVOR x86_DEBUG_STATE64
#else
typedef arm_unified_thread_state_t GumDarwinUnifiedThreadState;
# if GLIB_SIZEOF_VOID_P == 4
typedef arm_thread_state32_t GumDarwinNativeThreadState;
typedef arm_neon_state32_t GumDarwinNativeNeonState;
typedef arm_debug_state32_t GumDarwinNativeDebugState;
# else
typedef arm_thread_state64_t GumDarwinNativeThreadState;
typedef arm_neon_state64_t GumDarwinNativeNeonState;
typedef arm_debug_state64_t GumDarwinNativeDebugState;
# endif
# define GUM_DARWIN_THREAD_STATE_COUNT ARM_UNIFIED_THREAD_STATE_COUNT
# define GUM_DARWIN_THREAD_STATE_FLAVOR ARM_UNIFIED_THREAD_STATE
# if GLIB_SIZEOF_VOID_P == 4
#  define GUM_DARWIN_NEON_STATE_COUNT ARM_NEON_STATE_COUNT
#  define GUM_DARWIN_NEON_STATE_FLAVOR ARM_NEON_STATE
#  define GUM_DARWIN_DEBUG_STATE_COUNT ARM_DEBUG_STATE32_COUNT
#  define GUM_DARWIN_DEBUG_STATE_FLAVOR ARM_DEBUG_STATE32
# else
#  define GUM_DARWIN_NEON_STATE_COUNT ARM_NEON_STATE64_COUNT
#  define GUM_DARWIN_NEON_STATE_FLAVOR ARM_NEON_STATE64
#  define GUM_DARWIN_DEBUG_STATE_COUNT ARM_DEBUG_STATE64_COUNT
#  define GUM_DARWIN_DEBUG_STATE_FLAVOR ARM_DEBUG_STATE64
# endif
#endif

typedef struct _GumDarwinAllImageInfos GumDarwinAllImageInfos;
typedef struct _GumDarwinMappingDetails GumDarwinMappingDetails;
typedef struct _GumDarwinPThreadIter GumDarwinPThreadIter;
typedef struct _GumDarwinPThreadSpec GumDarwinPThreadSpec;
typedef struct _GumDarwinPThreadList GumDarwinPThreadList;
typedef struct _GumDarwinPThread GumDarwinPThread;
typedef struct _GumDarwinImageSnapshot GumDarwinImageSnapshot;
typedef struct _GumDarwinImage GumDarwinImage;
typedef struct _GumDarwinImageIter GumDarwinImageIter;

TAILQ_HEAD (_GumDarwinPThreadList, _GumDarwinPThread);

struct _GumDarwinAllImageInfos
{
  gint format;

  GumAddress info_array_address;
  gsize info_array_count;
  gsize info_array_size;

  GumAddress notification_address;

  gboolean libsystem_initialized;

  GumAddress dyld_image_load_address;

  GumAddress dyld_all_image_infos_address;

  guint8 shared_cache_uuid[16];
  GumAddress shared_cache_base_address;
  GumAddress shared_cache_slide;
};

struct _GumDarwinMappingDetails
{
  gchar path[MAXPATHLEN];

  guint64 offset;
  guint64 size;
};

struct _GumDarwinPThreadIter
{
  gpointer node;
  const GumDarwinPThreadSpec * spec;
};

struct _GumDarwinPThreadSpec
{
  struct _GumDarwinPThreadList * thread_list;
  os_unfair_lock_t thread_list_lock;

  guint mach_port_offset;
  guint name_offset;
  guint start_routine_offset;
  guint start_parameter_offset;
};

struct _GumDarwinPThread
{
  long sig;
  gpointer __cleanup_stack;
  TAILQ_ENTRY (_GumDarwinPThread) tl_plist;
};

struct _GumDarwinImage
{
  const gchar * path;
  GumMemoryRange range;
};

struct _GumDarwinImageIter
{
  const GumDarwinImageSnapshot * snapshot;
  gint index;
};

GUM_API gboolean gum_darwin_check_xnu_version (guint major, guint minor,
    guint micro);
GUM_API gboolean gum_darwin_is_debugger_mapping_enforced (void);

GUM_API guint8 * gum_darwin_read (mach_port_t task, GumAddress address,
    gsize len, gsize * n_bytes_read);
GUM_API gboolean gum_darwin_write (mach_port_t task, GumAddress address,
    const guint8 * bytes, gsize len);
GUM_API gboolean gum_darwin_cpu_type_from_pid (pid_t pid,
    GumCpuType * cpu_type);
GUM_API gboolean gum_darwin_query_ptrauth_support (mach_port_t task,
    GumPtrauthSupport * ptrauth_support);
GUM_API gboolean gum_darwin_query_page_size (mach_port_t task,
    guint * page_size);
GUM_API const gchar * gum_darwin_query_sysroot (void);
GUM_API gboolean gum_darwin_query_hardened (void);
GUM_API gboolean gum_darwin_query_all_image_infos (mach_port_t task,
    GumDarwinAllImageInfos * infos, GError ** error);
GUM_API gboolean gum_darwin_query_mapped_address (mach_port_t task,
    GumAddress address, GumDarwinMappingDetails * details);
GUM_API gboolean gum_darwin_query_protection (mach_port_t task,
    GumAddress address, GumPageProtection * prot);
GUM_API gboolean gum_darwin_query_shared_cache_range (mach_port_t task,
    GumMemoryRange * range);
GUM_API GumAddress gum_darwin_find_entrypoint (mach_port_t task);
GUM_API gboolean gum_darwin_modify_thread (mach_port_t thread,
    GumModifyThreadFunc func, gpointer user_data, GumModifyThreadFlags flags);
GUM_API void gum_darwin_enumerate_threads (mach_port_t task,
    GumFoundThreadFunc func, gpointer user_data, GumThreadFlags flags);
GUM_API void gum_darwin_pthread_iter_init (GumDarwinPThreadIter * iter,
    const GumDarwinPThreadSpec * spec);
GUM_API gboolean gum_darwin_pthread_iter_next (GumDarwinPThreadIter * self,
    pthread_t * thread);
GUM_API GumModule * gum_darwin_find_module_by_name (mach_port_t task,
    const gchar * name);
GUM_API void gum_darwin_enumerate_modules (mach_port_t task,
    GumFoundModuleFunc func, gpointer user_data);
GUM_API GumDarwinImageSnapshot * gum_darwin_snapshot_images (mach_port_t task,
    GError ** error);
GUM_API GumDarwinImageSnapshot * gum_darwin_image_snapshot_ref (
    GumDarwinImageSnapshot * snapshot);
GUM_API void gum_darwin_image_snapshot_unref (
    GumDarwinImageSnapshot * snapshot);
GUM_API gchar * gum_darwin_image_snapshot_infer_sysroot (
    const GumDarwinImageSnapshot * self);
GUM_API void gum_darwin_image_iter_init (GumDarwinImageIter * iter,
    const GumDarwinImageSnapshot * snapshot);
GUM_API gboolean gum_darwin_image_iter_next (GumDarwinImageIter * iter,
    const GumDarwinImage ** image);
GUM_API void gum_darwin_enumerate_ranges (mach_port_t task,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
GUM_API gboolean gum_darwin_query_thread_state (mach_port_t thread,
    GumThreadState * state);
GUM_API gboolean gum_darwin_query_thread_cpu_context (mach_port_t thread,
    GumCpuContext * ctx);
GUM_API mach_port_t gum_darwin_query_pthread_port (pthread_t thread,
    const GumDarwinPThreadSpec * spec);
GUM_API const gchar * gum_darwin_query_pthread_name (pthread_t thread,
    const GumDarwinPThreadSpec * spec);
GUM_API gpointer gum_darwin_query_pthread_start_routine (pthread_t thread,
    const GumDarwinPThreadSpec * spec);
GUM_API gpointer gum_darwin_query_pthread_start_parameter (pthread_t thread,
    const GumDarwinPThreadSpec * spec);
GUM_API void gum_darwin_lock_pthread_list (const GumDarwinPThreadSpec * spec);
GUM_API void gum_darwin_unlock_pthread_list (const GumDarwinPThreadSpec * spec);
GUM_API const GumDarwinPThreadSpec * gum_darwin_query_pthread_spec (void);

GUM_API gboolean gum_darwin_find_slide (GumAddress module_address,
    const guint8 * module, gsize module_size, gint64 * slide);
GUM_API gboolean gum_darwin_find_command (guint id, const guint8 * module,
    gsize module_size, gpointer * command);

GUM_API void gum_darwin_parse_unified_thread_state (
    const GumDarwinUnifiedThreadState * ts, GumCpuContext * ctx);
GUM_API void gum_darwin_parse_native_thread_state (
    const GumDarwinNativeThreadState * ts, GumCpuContext * ctx);
#if defined (__arm__) || defined (__aarch64__)
GUM_API void gum_darwin_parse_native_neon_state (
    const GumDarwinNativeNeonState * ns, GumCpuContext * ctx);
#endif
GUM_API void gum_darwin_unparse_unified_thread_state (
    const GumCpuContext * ctx, GumDarwinUnifiedThreadState * ts);
GUM_API void gum_darwin_unparse_native_thread_state (
    const GumCpuContext * ctx, GumDarwinNativeThreadState * ts);
#if defined (__arm__) || defined (__aarch64__)
GUM_API void gum_darwin_unparse_native_neon_state (
    const GumCpuContext * ctx, GumDarwinNativeNeonState * ns);
#endif

GUM_API GumPageProtection gum_page_protection_from_mach (vm_prot_t native_prot);
GUM_API vm_prot_t gum_page_protection_to_mach (GumPageProtection prot);

GUM_API const char * gum_symbol_name_from_darwin (const char * s);

GUM_API mach_port_t gum_kernel_get_task (void);

G_END_DECLS

#endif
