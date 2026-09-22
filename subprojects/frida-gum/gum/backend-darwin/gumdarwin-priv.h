/*
 * Copyright (C) 2021-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_PRIV_H__
#define __GUM_DARWIN_PRIV_H__

#include <glib.h>
#include <mach/mach.h>
#include <sys/param.h>

G_BEGIN_DECLS

#define DYLD_INFO_COUNT 5
#define DYLD_INFO_LEGACY_COUNT 1
#define DYLD_INFO_32_COUNT 3
#define DYLD_INFO_64_COUNT 5
#define DYLD_IMAGE_INFO_32_SIZE 12
#define DYLD_IMAGE_INFO_64_SIZE 24

#define GUM_DARWIN_MAX_THREAD_NAME_SIZE 64

typedef union _DyldInfo DyldInfo;
typedef struct _DyldInfoLegacy DyldInfoLegacy;
typedef struct _DyldInfo32 DyldInfo32;
typedef struct _DyldInfo64 DyldInfo64;
typedef struct _DyldAllImageInfos32 DyldAllImageInfos32;
typedef struct _DyldAllImageInfos64 DyldAllImageInfos64;
typedef struct _DyldImageInfo32 DyldImageInfo32;
typedef struct _DyldImageInfo64 DyldImageInfo64;
typedef struct _GumDyldCacheHeaderV0 GumDyldCacheHeaderV0;
typedef struct _GumDyldCacheMappingInfo GumDyldCacheMappingInfo;
typedef struct _GumPagePlanBuilder GumPagePlanBuilder;
typedef struct _GumPageBlock GumPageBlock;

struct _DyldInfoLegacy
{
  guint32 all_image_info_addr;
};

struct _DyldInfo32
{
  guint32 all_image_info_addr;
  guint32 all_image_info_size;
  gint32 all_image_info_format;
};

struct _DyldInfo64
{
  guint64 all_image_info_addr;
  guint64 all_image_info_size;
  gint32 all_image_info_format;
};

union _DyldInfo
{
  DyldInfoLegacy info_legacy;
  DyldInfo32 info_32;
  DyldInfo64 info_64;
};

struct _DyldAllImageInfos32
{
  guint32 version;
  guint32 info_array_count;
  guint32 info_array;
  guint32 notification;
  guint8 process_detached_from_shared_region;
  guint8 libsystem_initialized;
  guint32 dyld_image_load_address;
  guint32 jit_info;
  guint32 dyld_version;
  guint32 error_message;
  guint32 termination_flags;
  guint32 core_symbolication_shm_page;
  guint32 system_order_flag;
  guint32 uuid_array_count;
  guint32 uuid_array;
  guint32 dyld_all_image_infos_address;
  guint32 initial_image_count;
  guint32 error_kind;
  guint32 error_client_of_dylib_path;
  guint32 error_target_dylib_path;
  guint32 error_symbol;
  guint32 shared_cache_slide;
  guint8 shared_cache_uuid[16];
  guint32 shared_cache_base_address;
  volatile guint64 info_array_change_timestamp;
  guint32 dyld_path;
  guint32 notify_mach_ports[8];
  guint32 reserved[5];
  guint32 compact_dyld_image_info_addr;
  guint32 compact_dyld_image_info_size;
  guint32 platform;
};

struct _DyldAllImageInfos64
{
  guint32 version;
  guint32 info_array_count;
  guint64 info_array;
  guint64 notification;
  guint8 process_detached_from_shared_region;
  guint8 libsystem_initialized;
  guint32 padding;
  guint64 dyld_image_load_address;
  guint64 jit_info;
  guint64 dyld_version;
  guint64 error_message;
  guint64 termination_flags;
  guint64 core_symbolication_shm_page;
  guint64 system_order_flag;
  guint64 uuid_array_count;
  guint64 uuid_array;
  guint64 dyld_all_image_infos_address;
  guint64 initial_image_count;
  guint64 error_kind;
  guint64 error_client_of_dylib_path;
  guint64 error_target_dylib_path;
  guint64 error_symbol;
  guint64 shared_cache_slide;
  guint8 shared_cache_uuid[16];
  guint64 shared_cache_base_address;
  volatile guint64 info_array_change_timestamp;
  guint64 dyld_path;
  guint32 notify_mach_ports[8];
  guint64 reserved[9];
  guint64 compact_dyld_image_info_addr;
  guint64 compact_dyld_image_info_size;
  guint32 platform;
};

struct _DyldImageInfo32
{
  guint32 image_load_address;
  guint32 image_file_path;
  guint32 image_file_mod_date;
};

struct _DyldImageInfo64
{
  guint64 image_load_address;
  guint64 image_file_path;
  guint64 image_file_mod_date;
};

struct _GumDyldCacheHeaderV0
{
  gchar magic[16];
  guint32 mapping_offset;
  guint32 mapping_count;
  guint32 images_offset;
  guint32 images_count;
  guint64 dyld_base_address;
};

struct _GumDyldCacheMappingInfo
{
  guint64 address;
  guint64 size;
  guint64 file_offset;
  guint32 max_prot;
  guint32 init_prot;
};

#ifndef PROC_PIDREGIONPATHINFO2
# define PROC_PIDREGIONPATHINFO2 22
#endif

#ifndef PROC_INFO_CALL_PIDINFO

# define PROC_INFO_CALL_PIDINFO 0x2
# define PROC_PIDREGIONINFO     7
# define PROC_PIDREGIONPATHINFO 8

struct vinfo_stat
{
  uint32_t vst_dev;
  uint16_t vst_mode;
  uint16_t vst_nlink;
  uint64_t vst_ino;
  uid_t vst_uid;
  gid_t vst_gid;
  int64_t vst_atime;
  int64_t vst_atimensec;
  int64_t vst_mtime;
  int64_t vst_mtimensec;
  int64_t vst_ctime;
  int64_t vst_ctimensec;
  int64_t vst_birthtime;
  int64_t vst_birthtimensec;
  off_t vst_size;
  int64_t vst_blocks;
  int32_t vst_blksize;
  uint32_t vst_flags;
  uint32_t vst_gen;
  uint32_t vst_rdev;
  int64_t vst_qspare[2];
};

struct vnode_info
{
  struct vinfo_stat vi_stat;
  int vi_type;
  int vi_pad;
  fsid_t vi_fsid;
};

struct vnode_info_path
{
  struct vnode_info vip_vi;
  char vip_path[MAXPATHLEN];
};

struct proc_regioninfo
{
  uint32_t pri_protection;
  uint32_t pri_max_protection;
  uint32_t pri_inheritance;
  uint32_t pri_flags;
  uint64_t pri_offset;
  uint32_t pri_behavior;
  uint32_t pri_user_wired_count;
  uint32_t pri_user_tag;
  uint32_t pri_pages_resident;
  uint32_t pri_pages_shared_now_private;
  uint32_t pri_pages_swapped_out;
  uint32_t pri_pages_dirtied;
  uint32_t pri_ref_count;
  uint32_t pri_shadow_depth;
  uint32_t pri_share_mode;
  uint32_t pri_private_pages_resident;
  uint32_t pri_shared_pages_resident;
  uint32_t pri_obj_id;
  uint32_t pri_depth;
  uint64_t pri_address;
  uint64_t pri_size;
};

struct proc_regionwithpathinfo
{
  struct proc_regioninfo prp_prinfo;
  struct vnode_info_path prp_vip;
};

struct _GumPagePlanBuilder
{
  GArray * page_blocks;
};

struct _GumPageBlock
{
  gpointer start;
  gpointer end;
  GByteArray * bytes;
};

#endif

G_GNUC_INTERNAL gboolean _gum_darwin_fill_file_mapping (gint pid,
    mach_vm_address_t address, GumFileMapping * file,
    struct proc_regionwithpathinfo * region);
G_GNUC_INTERNAL void _gum_darwin_clamp_range_size (GumMemoryRange * range,
    const GumFileMapping * file);

G_GNUC_INTERNAL void _gum_page_plan_builder_init (GumPagePlanBuilder * self);
G_GNUC_INTERNAL void _gum_page_plan_builder_free (GumPagePlanBuilder * self);
G_GNUC_INTERNAL void _gum_page_plan_builder_add_page (GumPagePlanBuilder * self,
    gpointer target_page);
G_GNUC_INTERNAL void _gum_page_plan_builder_add_pages (
    GumPagePlanBuilder * self, gpointer base, gsize n_pages);
G_GNUC_INTERNAL gboolean _gum_page_plan_builder_post (
    GumPagePlanBuilder * self);

G_END_DECLS

#endif
