/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumpagepool.h"
#include "gummemory.h"

#define DEFAULT_PROTECT_MODE    GUM_PROTECT_MODE_ABOVE
#define MIN_POOL_SIZE           2
#define MAX_POOL_SIZE           G_MAXUINT32
#define DEFAULT_POOL_SIZE       G_MAXUINT16
#define DEFAULT_FRONT_ALIGNMENT 16

typedef struct _AlignmentCriteria AlignmentCriteria;
typedef struct _TailAlignResult   TailAlignResult;

struct _GumPagePool
{
  GObject parent;

  gboolean disposed;

  guint page_size;
  GumProtectMode protect_mode;
  guint size;
  guint front_alignment;

  guint available;
  guint cur_offset;
  guint8 * pool;
  guint8 * pool_end;
  GumBlockDetails * block_details;
};

enum
{
  PROP_0,
  PROP_PAGE_SIZE,
  PROP_PROTECT_MODE,
  PROP_SIZE,
  PROP_FRONT_ALIGNMENT
};

struct _AlignmentCriteria
{
  gsize front;
  gsize tail;
};

struct _TailAlignResult
{
  gpointer aligned_ptr;
  gpointer next_tail_ptr;
  gsize gap_size;
};

static void gum_page_pool_constructed (GObject * object);
static void gum_page_pool_finalize (GObject * object);
static void gum_page_pool_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_page_pool_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gint find_start_index_with_n_free_pages (GumPagePool * self,
    guint n_pages);
static gint find_start_index_for_address (GumPagePool * self, const guint8 * p);

static guint num_pages_needed_for (GumPagePool * self, guint size);

static gpointer claim_n_pages_at (GumPagePool * self, guint n_pages,
    guint start_index);
static gpointer release_n_pages_at (GumPagePool * self, guint n_pages,
    guint start_index);

static void tail_align (gpointer ptr, gsize size,
    const AlignmentCriteria * criteria, TailAlignResult * result);

G_DEFINE_TYPE (GumPagePool, gum_page_pool, G_TYPE_OBJECT)

static void
gum_page_pool_class_init (GumPagePoolClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gum_page_pool_constructed;
  object_class->finalize = gum_page_pool_finalize;
  object_class->get_property = gum_page_pool_get_property;
  object_class->set_property = gum_page_pool_set_property;

  g_object_class_install_property (object_class, PROP_PAGE_SIZE,
      g_param_spec_uint ("page-size", "Page Size", "System Page Size",
      4096, G_MAXUINT, 4096,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PROTECT_MODE,
      g_param_spec_uint ("protect-mode", "Protect Mode", "Protect Mode",
      0, G_MAXUINT, DEFAULT_PROTECT_MODE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_SIZE,
      g_param_spec_uint ("size", "Size", "Size in number of pages",
      MIN_POOL_SIZE, MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRONT_ALIGNMENT,
      g_param_spec_uint ("front-alignment", "Front Alignment",
      "Front alignment requirement",
      1, 64, DEFAULT_FRONT_ALIGNMENT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gum_page_pool_init (GumPagePool * self)
{
  self->page_size = gum_query_page_size ();
  self->protect_mode = DEFAULT_PROTECT_MODE;
  self->size = DEFAULT_POOL_SIZE;
  self->front_alignment = DEFAULT_FRONT_ALIGNMENT;
}

static void
gum_page_pool_constructed (GObject * object)
{
  GumPagePool * self = GUM_PAGE_POOL (object);

  self->available = self->size;
  self->pool = gum_memory_allocate (NULL, self->size * self->page_size,
      self->page_size, GUM_PAGE_NO_ACCESS);
  gum_memory_recommit (self->pool, self->size * self->page_size,
      GUM_PAGE_NO_ACCESS);
  self->pool_end = self->pool + (self->size * self->page_size);
  self->block_details = g_malloc0 (self->size * sizeof (GumBlockDetails));
}

static void
gum_page_pool_finalize (GObject * object)
{
  GumPagePool * self = GUM_PAGE_POOL (object);

  g_free (self->block_details);
  gum_memory_free (self->pool, self->size * self->page_size);

  G_OBJECT_CLASS (gum_page_pool_parent_class)->finalize (object);
}

static void
gum_page_pool_get_property (GObject * object,
                            guint property_id,
                            GValue * value,
                            GParamSpec * pspec)
{
  GumPagePool * self = GUM_PAGE_POOL (object);

  switch (property_id)
  {
    case PROP_PAGE_SIZE:
      g_value_set_uint (value, self->page_size);
      break;
    case PROP_PROTECT_MODE:
      g_value_set_uint (value, self->protect_mode);
      break;
    case PROP_SIZE:
      g_value_set_uint (value, self->size);
      break;
    case PROP_FRONT_ALIGNMENT:
      g_value_set_uint (value, self->front_alignment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_page_pool_set_property (GObject * object,
                            guint property_id,
                            const GValue * value,
                            GParamSpec * pspec)
{
  GumPagePool * self = GUM_PAGE_POOL (object);

  switch (property_id)
  {
    case PROP_PROTECT_MODE:
      self->protect_mode = g_value_get_uint (value);
      break;
    case PROP_SIZE:
      self->size = g_value_get_uint (value);
      break;
    case PROP_FRONT_ALIGNMENT:
      self->front_alignment = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumPagePool *
gum_page_pool_new (GumProtectMode protect_mode,
                   guint n_pages)
{
  return g_object_new (GUM_TYPE_PAGE_POOL,
      "protect-mode", protect_mode,
      "size", n_pages,
      NULL);
}

gpointer
gum_page_pool_try_alloc (GumPagePool * self,
                         guint size)
{
  gpointer result = NULL;
  guint n_pages;

  g_assert (size != 0);

  n_pages = num_pages_needed_for (self, size);

  if (n_pages <= self->available)
  {
    gint start_index;

    start_index = find_start_index_with_n_free_pages (self, n_pages);
    if (start_index >= 0)
    {
      guint8 * page_start;
      AlignmentCriteria align_criteria;
      TailAlignResult align_result;
      guint i;

      page_start = claim_n_pages_at (self, n_pages, start_index);

      align_criteria.front = self->front_alignment;
      align_criteria.tail = self->page_size;
      tail_align (page_start, size, &align_criteria, &align_result);

      for (i = start_index; i < start_index + n_pages; i++)
      {
        GumBlockDetails * details = &self->block_details[i];

        details->address = align_result.aligned_ptr;
        details->size = size;

        details->guard = page_start + ((n_pages - 1) * self->page_size);
        details->guard_size = self->page_size;
      }

      result = align_result.aligned_ptr;
    }
  }

  return result;
}

gboolean
gum_page_pool_try_free (GumPagePool * self,
                        gpointer mem)
{
  gint start_index;
  guint n_pages;

  start_index = find_start_index_for_address (self, mem);
  if (start_index < 0)
    return FALSE;

  n_pages = num_pages_needed_for (self, self->block_details[start_index].size);
  release_n_pages_at (self, n_pages, start_index);

  return TRUE;
}

guint
gum_page_pool_peek_available (GumPagePool * self)
{
  return self->available;
}

guint
gum_page_pool_peek_used (GumPagePool * self)
{
  return self->size - self->available;
}

void
gum_page_pool_get_bounds (GumPagePool * self,
                          guint8 ** lower,
                          guint8 ** upper)
{
  *lower = self->pool;
  *upper = self->pool_end;
}

gboolean
gum_page_pool_query_block_details (GumPagePool * self,
                                   gconstpointer mem,
                                   GumBlockDetails * details)
{
  gint start_index;

  start_index = find_start_index_for_address (self, mem);
  if (start_index < 0)
    return FALSE;

  *details = self->block_details[start_index];
  return TRUE;
}

static gint
find_start_index_with_n_free_pages (GumPagePool * self,
                                    guint n_pages)
{
  gint result = -1;
  guint first_index;
  guint i, n;

  first_index = self->cur_offset;

start_over:

  for (i = first_index, n = 0; i < self->size && n < n_pages; i++)
  {
    if (!self->block_details[i].allocated)
      n++;
    else
      n = 0;
  }

  if (n == n_pages)
  {
    result = i - n_pages;
  }
  else if (first_index != 0)
  {
    first_index = 0;
    goto start_over;
  }

  return result;
}

static gint
find_start_index_for_address (GumPagePool * self,
                              const guint8 * p)
{
  if (p < self->pool || p > self->pool_end)
    return -1;

  return (p - self->pool) / self->page_size;
}

static guint
num_pages_needed_for (GumPagePool * self,
                      guint size)
{
  guint n_pages;

  n_pages = (size / self->page_size) + 1;
  if (size % self->page_size != 0)
    n_pages++;

  return n_pages;
}

#define POOL_ADDRESS_FROM_PAGE_INDEX(n) (self->pool + (n * self->page_size))

static gpointer
claim_n_pages_at (GumPagePool * self,
                  guint n_pages,
                  guint start_index)
{
  gpointer start_address;
  guint i;

  start_address = POOL_ADDRESS_FROM_PAGE_INDEX (start_index);

  self->cur_offset = start_index + n_pages;
  self->available -= n_pages;

  for (i = start_index; i < start_index + n_pages; i++)
  {
    GumBlockDetails * details = &self->block_details[i];

    details->allocated = TRUE;
  }

  gum_mprotect (start_address, (n_pages - 1) * self->page_size,
      GUM_PAGE_READ | GUM_PAGE_WRITE);
  return start_address;
}

static gpointer
release_n_pages_at (GumPagePool * self,
                    guint n_pages,
                    guint start_index)
{
  gpointer start_address;
  guint i;

  self->available += n_pages;

  for (i = start_index; i < start_index + n_pages; i++)
  {
    GumBlockDetails * details = &self->block_details[i];

    details->allocated = FALSE;
  }

  start_address = POOL_ADDRESS_FROM_PAGE_INDEX (start_index);
  gum_mprotect (start_address, n_pages - 1, GUM_PAGE_NO_ACCESS);

  return start_address;
}

static void
tail_align (gpointer ptr,
            gsize size,
            const AlignmentCriteria * criteria,
            TailAlignResult * result)
{
  gsize unaligned_start_address, unaligned_end_address;
  gsize next_tail_boundary;
  gsize aligned_start_address, aligned_end_address;

  unaligned_start_address = GPOINTER_TO_SIZE (ptr);
  unaligned_end_address = unaligned_start_address + size - 1;
  next_tail_boundary = ((unaligned_end_address / criteria->tail) + 1)
      * criteria->tail;

  aligned_start_address = ((next_tail_boundary - size) / criteria->front)
      * criteria->front;
  if (aligned_start_address < unaligned_start_address)
  {
    aligned_start_address += criteria->tail;
    next_tail_boundary += criteria->tail;
  }
  aligned_end_address = aligned_start_address + size - 1;

  result->aligned_ptr = GSIZE_TO_POINTER (aligned_start_address);
  result->next_tail_ptr = GSIZE_TO_POINTER (next_tail_boundary);
  result->gap_size = next_tail_boundary - (aligned_end_address + 1);
}
