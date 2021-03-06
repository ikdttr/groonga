/* -*- c-basic-offset: 2 -*- */
/* Copyright(C) 2009 Brazil

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef GRN_IO_H
#define GRN_IO_H

#ifndef GROONGA_H
#include "groonga_in.h"
#endif /* GROONGA_H */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef WIN32
#ifdef WIN32_FMO_EACH
#define GRN_IO_FILE_SIZE               1073741824UL
#else /* FMO_EACH */
#define GRN_IO_FILE_SIZE               134217728L
#endif /* FMO_EACH */
#define GRN_IO_COPY grn_io_rdonly
#define GRN_IO_UPDATE grn_io_wronly
#else /* WIN32 */
#define GRN_IO_FILE_SIZE               1073741824UL
#define GRN_IO_COPY grn_io_rdwr
#define GRN_IO_UPDATE grn_io_rdwr
#endif /* WIN32 */

typedef enum {
  grn_io_rdonly,
  grn_io_wronly,
  grn_io_rdwr
} grn_io_rw_mode;

typedef enum {
  grn_io_auto,
  grn_io_manual
} grn_io_mode;

/**** grn_io ****/

typedef struct _grn_io grn_io;

typedef struct {
  grn_io *io;
  grn_ctx *ctx;
  uint8_t mode;
  uint8_t tiny_p;
  uint32_t pseg;
  uint32_t segment;
  uint32_t offset;
  uint32_t size;
  uint32_t nseg;
  off_t pos;
  void *addr;
  uint32_t diff;
  int32_t cached;
#if defined(WIN32) && defined(WIN32_FMO_EACH)
  HANDLE fmo;
#endif /* defined(WIN32) && defined(WIN32_FMO_EACH) */
} grn_io_win;

typedef struct {
  void *map;
  uint32_t nref;
  uint32_t count;
#if defined(WIN32) && defined(WIN32_FMO_EACH)
  HANDLE fmo;
#endif /* defined(WIN32) && defined(WIN32_FMO_EACH) */
} grn_io_mapinfo;

typedef struct _grn_io_array_info grn_io_array_info;

struct _grn_io {
  char path[PATH_MAX];
  struct _grn_io_header *header;
  byte *user_header;
  grn_io_mapinfo *maps;
  uint32_t base;
  uint32_t base_seg;
  grn_io_mode mode;
  struct _grn_io_fileinfo *fis;
  grn_io_array_info *ainfo;
  uint32_t max_map_seg;
  uint32_t nmaps;
  uint32_t nref;
  uint32_t count;
  uint8_t flags;
  uint32_t *lock;
};

grn_io *grn_io_create(grn_ctx *ctx, const char *path, uint32_t header_size, uint32_t segment_size,
		      uint32_t max_segment, grn_io_mode mode, unsigned int flags);
grn_io *grn_io_open(grn_ctx *ctx, const char *path, grn_io_mode mode);
grn_rc grn_io_close(grn_ctx *ctx, grn_io *io);
grn_rc grn_io_remove(grn_ctx *ctx, const char *path);
grn_rc grn_io_size(grn_ctx *ctx, grn_io *io, uint64_t *size);
grn_rc grn_io_rename(grn_ctx *ctx, const char *old_name, const char *new_name);
void *grn_io_header(grn_io *io);

void *grn_io_win_map(grn_io *io, grn_ctx *ctx, grn_io_win *iw, uint32_t segment,
		     uint32_t offset, uint32_t size, grn_io_rw_mode mode);
grn_rc grn_io_win_mapv(grn_io_win **list, grn_ctx *ctx, int nent);
grn_rc grn_io_win_unmap(grn_io_win *iw);

void *grn_io_win_map2(grn_io *io, grn_ctx *ctx, grn_io_win *iw, uint32_t segment,
                      uint32_t offset, uint32_t size, grn_io_rw_mode mode);
grn_rc grn_io_win_unmap2(grn_io_win *iw);

typedef struct _grn_io_ja_einfo grn_io_ja_einfo;
typedef struct _grn_io_ja_ehead grn_io_ja_ehead;

struct _grn_io_ja_einfo {
  uint32_t pos;
  uint32_t size;
};

struct _grn_io_ja_ehead {
  uint32_t size;
  uint32_t key;
};

grn_rc grn_io_read_ja(grn_io *io, grn_ctx *ctx, grn_io_ja_einfo *einfo, uint32_t epos,
                      uint32_t key, uint32_t segment, uint32_t offset,
                      void **value, uint32_t *value_len);
grn_rc grn_io_write_ja(grn_io *io, grn_ctx *ctx,
                       uint32_t key, uint32_t segment, uint32_t offset,
                       void *value, uint32_t value_len);

grn_rc grn_io_write_ja_ehead(grn_io *io, grn_ctx *ctx, uint32_t key,
                             uint32_t segment, uint32_t offset, uint32_t value_len);

#define GRN_IO_MAX_RETRY               (0x10000)
#define GRN_IO_MAX_REF                 (0x80000000)

#define GRN_IO_EXPIRE_GTICK            (0x01)
#define GRN_IO_EXPIRE_SEGMENT          (0x02)
#define GRN_IO_TEMPORARY               (0x04)

void grn_io_seg_map_(grn_ctx *ctx, grn_io *io, uint32_t segno, grn_io_mapinfo *info);

/* arguments must be validated by caller;
 * io mustn't be NULL;
 * segno must be in valid range;
 * addr must be set NULL;
 */
#define GRN_IO_SEG_REF(io,segno,addr) {\
  grn_io_mapinfo *info = &(io)->maps[segno];\
  uint32_t nref, retry, *pnref = &info->nref;\
  if (io->flags & GRN_IO_EXPIRE_SEGMENT) {\
    if (io->flags & GRN_IO_EXPIRE_GTICK) {\
      for (retry = 0; !info->map || info->count != grn_gtick; retry++) {\
        GRN_ATOMIC_ADD_EX(pnref, 1, nref);\
        if (nref) {\
          GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
          if (retry >= GRN_IO_MAX_RETRY) {\
            GRN_LOG(ctx, GRN_LOG_CRIT, "deadlock detected! in GRN_IO_SEG_REF(%p, %u)", io, segno);\
            break;\
          }\
          GRN_FUTEX_WAIT(pnref);\
        } else {\
          info->count = grn_gtick;\
          if (!info->map) {\
            grn_io_seg_map_(ctx, io, segno, info);\
            if (!info->map) {\
              GRN_LOG(ctx, GRN_LOG_CRIT, "mmap failed! in GRN_IO_SEG_REF(%p, %u)", io, segno);\
            }\
          }\
          GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
          GRN_FUTEX_WAKE(pnref);\
          break;\
        }\
      }\
    } else {\
      for (retry = 0;; retry++) {\
        GRN_ATOMIC_ADD_EX(pnref, 1, nref);\
        if (nref >= GRN_IO_MAX_REF) {\
          GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
          if (retry >= GRN_IO_MAX_RETRY) {\
            GRN_LOG(ctx, GRN_LOG_CRIT, "deadlock detected!! in GRN_IO_SEG_REF(%p, %u, %u)", io, segno, nref);\
            *pnref = 0; /* force reset */ \
            break;\
          }\
          GRN_FUTEX_WAIT(pnref);\
          continue;\
        }\
        if (nref > 10000) {\
          GRN_LOG(ctx, GRN_LOG_ALERT, "strange nref value!! in GRN_IO_SEG_REF(%p, %u, %u)", io, segno, nref);\
        }\
        if (!info->map) {\
          if (nref) {\
            GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
            if (retry >= GRN_IO_MAX_RETRY) {\
              GRN_LOG(ctx, GRN_LOG_CRIT, "deadlock detected!!! in GRN_IO_SEG_REF(%p, %u, %u)", io, segno, nref);\
              break;\
            }\
            GRN_FUTEX_WAIT(pnref);\
            continue;\
          } else {\
            grn_io_seg_map_(ctx, io, segno, info);\
            if (!info->map) {\
              GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
              GRN_LOG(ctx, GRN_LOG_CRIT, "mmap failed!!! in GRN_IO_SEG_REF(%p, %u, %u)", io, segno, nref);\
            }\
            \
            GRN_FUTEX_WAKE(pnref);\
          }\
        }\
        break;\
      }\
      info->count = grn_gtick;\
    }\
  } else {\
    for (retry = 0; !info->map; retry++) {\
      GRN_ATOMIC_ADD_EX(pnref, 1, nref);\
      if (nref) {\
        GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
        if (retry >= GRN_IO_MAX_RETRY) {\
          GRN_LOG(ctx, GRN_LOG_CRIT, "deadlock detected!!!! in GRN_IO_SEG_REF(%p, %u)", io, segno);\
          break;\
        }\
        GRN_FUTEX_WAIT(pnref);\
      } else {\
        if (!info->map) {\
          grn_io_seg_map_(ctx, io, segno, info);\
          if (!info->map) {\
            GRN_LOG(ctx, GRN_LOG_CRIT, "mmap failed!!!! in GRN_IO_SEG_REF(%p, %u)", io, segno);\
          }\
        }\
        GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
        GRN_FUTEX_WAKE(pnref);\
        break;\
      }\
    }\
    info->count = grn_gtick;\
  }\
  addr = info->map;\
}

#define GRN_IO_SEG_UNREF(io,segno) {\
  if (GRN_IO_EXPIRE_SEGMENT ==\
      (io->flags & (GRN_IO_EXPIRE_GTICK|GRN_IO_EXPIRE_SEGMENT))) {\
    uint32_t nref, *pnref = &(io)->maps[segno].nref;\
    GRN_ATOMIC_ADD_EX(pnref, -1, nref);\
  }\
}

uint32_t grn_io_base_seg(grn_io *io);
const char *grn_io_path(grn_io *io);

typedef struct _grn_io_array_spec grn_io_array_spec;

struct _grn_io_array_spec {
  uint32_t w_of_element;
  uint32_t max_n_segments;
};

struct _grn_io_array_info {
  uint32_t w_of_elm_in_a_segment;
  uint32_t elm_mask_in_a_segment;
  uint32_t max_n_segments;
  uint32_t element_size;
  uint32_t *segments;
  void **addrs;
};

grn_io *grn_io_create_with_array(grn_ctx *ctx, const char *path, uint32_t header_size,
                                 uint32_t segment_size, grn_io_mode mode,
                                 int n_arrays, grn_io_array_spec *array_specs);

void *grn_io_array_at(grn_ctx *ctx, grn_io *io, uint32_t array, off_t offset, int *flags);

void grn_io_segment_alloc(grn_ctx *ctx, grn_io *io, grn_io_array_info *ai,
                          uint32_t lseg, int *flags, void **p);

grn_rc grn_io_lock(grn_ctx *ctx, grn_io *io, int timeout);
void grn_io_unlock(grn_io *io);
void grn_io_clear_lock(grn_io *io);
uint32_t grn_io_is_locked(grn_io *io);

#define GRN_IO_ARRAY_AT(io,array,offset,flags,res) do {\
  grn_io_array_info *ainfo = &(io)->ainfo[array];\
  uint32_t lseg = (offset) >> ainfo->w_of_elm_in_a_segment;\
  void **p_ = &ainfo->addrs[lseg];\
  if (!*p_) {\
    grn_io_segment_alloc(ctx, (io), ainfo, lseg, (flags), p_);\
    if (!*p_) { (res) = NULL; break; }\
  }\
  *((byte **)(&(res))) = (((byte *)*p_) + \
          (((offset) & ainfo->elm_mask_in_a_segment) * ainfo->element_size));\
} while (0)

#define GRN_IO_ARRAY_BIT_AT(io,array,offset,res) {\
  uint8_t *ptr_;\
  int flags_ = 0;\
  GRN_IO_ARRAY_AT((io), (array), ((offset) >> 3) + 1, &flags_, ptr_);\
  res = ptr_ ? ((*ptr_ >> ((offset) & 7)) & 1) : 0;\
}

#define GRN_IO_ARRAY_BIT_ON(io,array,offset) {\
  uint8_t *ptr_;\
  int flags_ = GRN_TABLE_ADD;\
  GRN_IO_ARRAY_AT((io), (array), ((offset) >> 3) + 1, &flags_, ptr_);\
  if (ptr_) { *ptr_ |= (1 << ((offset) & 7)); }\
}

#define GRN_IO_ARRAY_BIT_OFF(io,array,offset) {\
  uint8_t *ptr_;\
  int flags_ = GRN_TABLE_ADD;\
  GRN_IO_ARRAY_AT((io), (array), ((offset) >> 3) + 1, &flags_, ptr_);\
  if (ptr_) { *ptr_ &= ~(1 << ((offset) & 7)); }\
}

#define GRN_IO_ARRAY_BIT_FLIP(io,array,offset) {\
  uint8_t *ptr_;\
  int flags_ = GRN_TABLE_ADD;\
  GRN_IO_ARRAY_AT((io), (array), ((offset) >> 3) + 1, &flags_, ptr_);\
  if (ptr_) { *ptr_ ^= (1 << ((offset) & 7)); }\
}

#ifdef __cplusplus
}
#endif

void *grn_io_anon_map(grn_ctx *ctx, grn_io_mapinfo *mi, size_t length);
void grn_io_anon_unmap(grn_ctx *ctx, grn_io_mapinfo *mi, size_t length);
uint32_t grn_io_detect_type(grn_ctx *ctx, const char *path);
grn_rc grn_io_set_type(grn_io *io, uint32_t type);
uint32_t grn_io_get_type(grn_io *io);

grn_rc grn_io_init(void);
grn_rc grn_io_fin(void);
grn_id grn_dl_open(grn_ctx *ctx, const char *filename);
grn_rc grn_dl_close(grn_ctx *ctx, grn_id id);
void *grn_dl_sym(grn_ctx *ctx, grn_id id, const char *symbol);
grn_id grn_dl_get(grn_ctx *ctx, const char *filename);

uint32_t grn_io_expire(grn_ctx *ctx, grn_io *io, int count_thresh, uint32_t limit);
uint32_t grn_expire(grn_ctx *ctx, int count_thresh, uint32_t limit);

/* encode/decode */

#define GRN_B_ENC(v,p) \
{ \
  uint8_t *_p = (uint8_t *)p; \
  uint32_t _v = v; \
  if (_v < 0x8f) { \
    *_p++ = _v; \
  } else if (_v < 0x408f) { \
    _v -= 0x8f; \
    *_p++ = 0xc0 + (_v >> 8); \
    *_p++ = _v & 0xff; \
  } else if (_v < 0x20408f) { \
    _v -= 0x408f; \
    *_p++ = 0xa0 + (_v >> 16); \
    *_p++ = (_v >> 8) & 0xff; \
    *_p++ = _v & 0xff; \
  } else if (_v < 0x1020408f) { \
    _v -= 0x20408f; \
    *_p++ = 0x90 + (_v >> 24); \
    *_p++ = (_v >> 16) & 0xff; \
    *_p++ = (_v >> 8) & 0xff; \
    *_p++ = _v & 0xff; \
  } else { \
    *_p++ = 0x8f; \
    memcpy(_p, &_v, sizeof(uint32_t));\
    _p += sizeof(uint32_t); \
  } \
  p = _p; \
}

#define GRN_B_DEC(v,p) \
{ \
  uint8_t *_p = (uint8_t *)p; \
  uint32_t _v = *_p++; \
  switch (_v >> 4) { \
  case 0x08 : \
    if (_v == 0x8f) { \
      memcpy(&_v, _p, sizeof(uint32_t));\
      _p += sizeof(uint32_t); \
    } \
    break; \
  case 0x09 : \
    _v = (_v - 0x90) * 0x100 + *_p++; \
    _v = _v * 0x100 + *_p++; \
    _v = _v * 0x100 + *_p++ + 0x20408f; \
    break; \
  case 0x0a : \
  case 0x0b : \
    _v = (_v - 0xa0) * 0x100 + *_p++; \
    _v = _v * 0x100 + *_p++ + 0x408f; \
    break; \
  case 0x0c : \
  case 0x0d : \
  case 0x0e : \
  case 0x0f : \
    _v = (_v - 0xc0) * 0x100 + *_p++ + 0x8f; \
    break; \
  } \
  v = _v; \
  p = _p; \
}

#define GRN_B_SKIP(p) \
{ \
  uint8_t *_p = (uint8_t *)p; \
  uint32_t _v = *_p++; \
  switch (_v >> 4) { \
  case 0x08 : \
    if (_v == 0x8f) { \
      _p += sizeof(uint32_t); \
    } \
    break; \
  case 0x09 : \
    _p += 3; \
    break; \
  case 0x0a : \
  case 0x0b : \
    _p += 2; \
    break; \
  case 0x0c : \
  case 0x0d : \
  case 0x0e : \
  case 0x0f : \
    _p += 1; \
    break; \
  } \
  p = _p; \
}

#define GRN_B_COPY(p2,p1) \
{ \
  uint32_t size = 0, _v = *p1++; \
  *p2++ = _v; \
  switch (_v >> 4) { \
  case 0x08 : \
    size = (_v == 0x8f) ? 4 : 0; \
    break; \
  case 0x09 : \
    size = 3; \
    break; \
  case 0x0a : \
  case 0x0b : \
    size = 2; \
    break; \
  case 0x0c : \
  case 0x0d : \
  case 0x0e : \
  case 0x0f : \
    size = 1; \
    break; \
  } \
  while (size--) { *p2++ = *p1++; } \
}

#endif /* GRN_IO_H */
