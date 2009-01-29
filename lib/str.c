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
#include "groonga_in.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ctx.h"
#include "str.h"

#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE
#endif /* _ISOC99_SOURCE */
#include <math.h>

inline static int
grn_str_charlen_utf8(grn_ctx *ctx, const unsigned char *str, const unsigned char *end)
{
  /* MEMO: This function allows non-null-terminated string as str. */
  /*       But requires the end of string. */
  const unsigned char *p = str;
  if (!*p || p >= end) { return 0; }
  if (*p & 0x80) {
    int b, w;
    int size;
    for (b = 0x40, w = 0; b && (*p & b); b >>= 1, w++);
    if (!w) {
      GRN_LOG(grn_log_warning, "invalid utf8 string(1) on grn_str_charlen_utf8");
      return 0;
    }
    for (size = 1; w--; size++) {
      if (++p >= end || !*p || (*p & 0xc0) != 0x80) {
        GRN_LOG(grn_log_warning, "invalid utf8 string(2) on grn_str_charlen_utf8");
        return 0;
      }
    }
    return size;
  } else {
    return 1;
  }
  return 0;
}

unsigned int
grn_str_charlen(grn_ctx *ctx, const char *str, grn_encoding encoding)
{
  /* MEMO: This function requires null-terminated string as str.*/
  unsigned char *p = (unsigned char *) str;
  if (!*p) { return 0; }
  switch (encoding) {
  case grn_enc_euc_jp :
    if (*p & 0x80) {
      if (*(p + 1)) {
        return 2;
      } else {
        /* This is invalid character */
        GRN_LOG(grn_log_warning, "invalid euc-jp string end on grn_str_charlen");
        return 0;
      }
    }
    return 1;
    break;
  case grn_enc_utf8 :
    if (*p & 0x80) {
      int b, w;
      size_t size;
      for (b = 0x40, w = 0; b && (*p & b); b >>= 1, w++);
      if (!w) {
        GRN_LOG(grn_log_warning, "invalid utf8 string(1) on grn_str_charlen");
        return 0;
      }
      for (size = 1; w--; size++) {
        if (!*++p || (*p & 0xc0) != 0x80) {
          GRN_LOG(grn_log_warning, "invalid utf8 string(2) on grn_str_charlen");
          return 0;
        }
      }
      return size;
    } else {
      return 1;
    }
    break;
  case grn_enc_sjis :
    if (*p & 0x80) {
      /* we regard 0xa0 as JIS X 0201 KANA. adjusted to other tools. */
      if (0xa0 <= *p && *p <= 0xdf) {
        /* hankaku-kana */
        return 1;
      } else if (!(*(p + 1))) {
        /* This is invalid character */
        GRN_LOG(grn_log_warning, "invalid sjis string end on grn_str_charlen");
        return 0;
      } else {
        return 2;
      }
    } else {
      return 1;
    }
    break;
  default :
    return 1;
    break;
  }
  return 0;
}

int
grn_charlen(grn_ctx *ctx, const char *str, const char *end, grn_encoding encoding)
{
  /* MEMO: This function allows non-null-terminated string as str. */
  /*       But requires the end of string. */
  unsigned char *p = (unsigned char *) str;
  if (p >= (unsigned char *)end) { return 0; }
  switch (encoding) {
  case grn_enc_euc_jp :
    if (*p & 0x80) {
      if ((p + 1) < (unsigned char *)end) {
        return 2;
      } else {
        /* This is invalid character */
        GRN_LOG(grn_log_warning, "invalid euc-jp string end on grn_charlen");
        return 0;
      }
    }
    return 1;
    break;
  case grn_enc_utf8 :
    return grn_str_charlen_utf8(ctx, p, (unsigned char *)end);
    break;
  case grn_enc_sjis :
    if (*p & 0x80) {
      /* we regard 0xa0 as JIS X 0201 KANA. adjusted to other tools. */
      if (0xa0 <= *p && *p <= 0xdf) {
        /* hankaku-kana */
        return 1;
      } else if (++p >= (unsigned char *)end) {
        /* This is invalid character */
        GRN_LOG(grn_log_warning, "invalid sjis string end on grn_charlen");
        return 0;
      } else {
        return 2;
      }
    } else {
      return 1;
    }
    break;
  default :
    return 1;
    break;
  }
  return 0;
}

static unsigned char symbol[] = {
  ',', '.', 0, ':', ';', '?', '!', 0, 0, 0, '`', 0, '^', '~', '_', 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, '-', '-', '/', '\\', 0, 0, '|', 0, 0, 0, '\'', 0,
  '"', '(', ')', 0, 0, '[', ']', '{', '}', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  '+', '-', 0, 0, 0, '=', 0, '<', '>', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  '$', 0, 0, '%', '#', '&', '*', '@', 0, 0, 0, 0, 0, 0, 0, 0
};

inline static grn_rc
normalize_euc(grn_ctx *ctx, grn_str *nstr)
{
  static uint16_t hankana[] = {
    0xa1a1, 0xa1a3, 0xa1d6, 0xa1d7, 0xa1a2, 0xa1a6, 0xa5f2, 0xa5a1, 0xa5a3,
    0xa5a5, 0xa5a7, 0xa5a9, 0xa5e3, 0xa5e5, 0xa5e7, 0xa5c3, 0xa1bc, 0xa5a2,
    0xa5a4, 0xa5a6, 0xa5a8, 0xa5aa, 0xa5ab, 0xa5ad, 0xa5af, 0xa5b1, 0xa5b3,
    0xa5b5, 0xa5b7, 0xa5b9, 0xa5bb, 0xa5bd, 0xa5bf, 0xa5c1, 0xa5c4, 0xa5c6,
    0xa5c8, 0xa5ca, 0xa5cb, 0xa5cc, 0xa5cd, 0xa5ce, 0xa5cf, 0xa5d2, 0xa5d5,
    0xa5d8, 0xa5db, 0xa5de, 0xa5df, 0xa5e0, 0xa5e1, 0xa5e2, 0xa5e4, 0xa5e6,
    0xa5e8, 0xa5e9, 0xa5ea, 0xa5eb, 0xa5ec, 0xa5ed, 0xa5ef, 0xa5f3, 0xa1ab,
    0xa1eb
  };
  static unsigned char dakuten[] = {
    0xf4, 0, 0, 0, 0, 0xac, 0, 0xae, 0, 0xb0, 0, 0xb2, 0, 0xb4, 0, 0xb6, 0,
    0xb8, 0, 0xba, 0, 0xbc, 0, 0xbe, 0, 0xc0, 0, 0xc2, 0, 0, 0xc5, 0, 0xc7,
    0, 0xc9, 0, 0, 0, 0, 0, 0, 0xd0, 0, 0, 0xd3, 0, 0, 0xd6, 0, 0, 0xd9, 0,
    0, 0xdc
  };
  static unsigned char handaku[] = {
    0xd1, 0, 0, 0xd4, 0, 0, 0xd7, 0, 0, 0xda, 0, 0, 0xdd
  };
  int16_t *ch;
  const unsigned char *s, *s_, *e;
  unsigned char *d, *d0, *d_, b;
  uint_least8_t *cp, *ctypes, ctype;
  size_t size = nstr->orig_blen, length = 0;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size * 2 + 1))) {
    return GRN_NO_MEMORY_AVAILABLE;
  }
  d0 = (unsigned char *) nstr->norm;
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * 2 * sizeof(int16_t) + 1))) {
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size + 1))) {
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *) nstr->orig, d = d_ = d0; s < e; s++) {
    if ((*s & 0x80)) {
      if (((s + 1) < e) && (*(s + 1) & 0x80)) {
        unsigned char c1 = *s++, c2 = *s, c3 = 0;
        switch (c1 >> 4) {
        case 0x08 :
          if (c1 == 0x8e && 0xa0 <= c2 && c2 <= 0xdf) {
            uint16_t c = hankana[c2 - 0xa0];
            switch (c) {
            case 0xa1ab :
              if (d > d0 + 1 && d[-2] == 0xa5
                  && 0xa6 <= d[-1] && d[-1] <= 0xdb && (b = dakuten[d[-1] - 0xa6])) {
                *(d - 1) = b;
                if (ch) { ch[-1] += 2; s_ += 2; }
                continue;
              } else {
                *d++ = c >> 8; *d = c & 0xff;
              }
              break;
            case 0xa1eb :
              if (d > d0 + 1 && d[-2] == 0xa5
                  && 0xcf <= d[-1] && d[-1] <= 0xdb && (b = handaku[d[-1] - 0xcf])) {
                *(d - 1) = b;
                if (ch) { ch[-1] += 2; s_ += 2; }
                continue;
              } else {
                *d++ = c >> 8; *d = c & 0xff;
              }
              break;
            default :
              *d++ = c >> 8; *d = c & 0xff;
              break;
            }
            ctype = grn_str_katakana;
          } else {
            *d++ = c1; *d = c2;
            ctype = grn_str_others;
          }
          break;
        case 0x09 :
          *d++ = c1; *d = c2;
          ctype = grn_str_others;
          break;
        case 0x0a :
          switch (c1 & 0x0f) {
          case 1 :
            switch (c2) {
            case 0xbc :
              *d++ = c1; *d = c2;
              ctype = grn_str_katakana;
              break;
            case 0xb9 :
              *d++ = c1; *d = c2;
              ctype = grn_str_kanji;
              break;
            case 0xa1 :
              if (removeblankp) {
                if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
                continue;
              } else {
                *d = ' ';
                ctype = GRN_STR_BLANK|grn_str_symbol;
              }
              break;
            default :
              if (c2 >= 0xa4 && (c3 = symbol[c2 - 0xa4])) {
                *d = c3;
                ctype = grn_str_symbol;
              } else {
                *d++ = c1; *d = c2;
                ctype = grn_str_others;
              }
              break;
            }
            break;
          case 2 :
            *d++ = c1; *d = c2;
            ctype = grn_str_symbol;
            break;
          case 3 :
            c3 = c2 - 0x80;
            if ('a' <= c3 && c3 <= 'z') {
              ctype = grn_str_alpha;
              *d = c3;
            } else if ('A' <= c3 && c3 <= 'Z') {
              ctype = grn_str_alpha;
              *d = c3 + 0x20;
            } else if ('0' <= c3 && c3 <= '9') {
              ctype = grn_str_digit;
              *d = c3;
            } else {
              ctype = grn_str_others;
              *d++ = c1; *d = c2;
            }
            break;
          case 4 :
            *d++ = c1; *d = c2;
            ctype = grn_str_hiragana;
            break;
          case 5 :
            *d++ = c1; *d = c2;
            ctype = grn_str_katakana;
            break;
          case 6 :
          case 7 :
          case 8 :
            *d++ = c1; *d = c2;
            ctype = grn_str_symbol;
            break;
          default :
            *d++ = c1; *d = c2;
            ctype = grn_str_others;
            break;
          }
          break;
        default :
          *d++ = c1; *d = c2;
          ctype = grn_str_kanji;
          break;
        }
      } else {
        /* skip invalid character */
        continue;
      }
    } else {
      unsigned char c = *s;
      switch (c >> 4) {
      case 0 :
      case 1 :
        /* skip unprintable ascii */
        if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
        continue;
      case 2 :
        if (c == 0x20) {
          if (removeblankp) {
            if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
            continue;
          } else {
            *d = ' ';
            ctype = GRN_STR_BLANK|grn_str_symbol;
          }
        } else {
          *d = c;
          ctype = grn_str_symbol;
        }
        break;
      case 3 :
        *d = c;
        ctype = (c <= 0x39) ? grn_str_digit : grn_str_symbol;
        break;
      case 4 :
        *d = ('A' <= c) ? c + 0x20 : c;
        ctype = (c == 0x40) ? grn_str_symbol : grn_str_alpha;
        break;
      case 5 :
        *d = (c <= 'Z') ? c + 0x20 : c;
        ctype = (c <= 0x5a) ? grn_str_alpha : grn_str_symbol;
        break;
      case 6 :
        *d = c;
        ctype = (c == 0x60) ? grn_str_symbol : grn_str_alpha;
        break;
      case 7 :
        *d = c;
        ctype = (c <= 0x7a) ? grn_str_alpha : (c == 0x7f ? grn_str_others : grn_str_symbol);
        break;
      default :
        *d = c;
        ctype = grn_str_others;
        break;
      }
    }
    d++;
    length++;
    if (cp) { *cp++ = ctype; }
    if (ch) {
      *ch++ = (int16_t)(s + 1 - s_);
      s_ = s + 1;
      while (++d_ < d) { *ch++ = 0; }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}

#ifndef NO_NFKC
uint_least8_t grn_nfkc_ctype(const unsigned char *str);
const char *grn_nfkc_map1(const unsigned char *str);
const char *grn_nfkc_map2(const unsigned char *prefix, const unsigned char *suffix);

inline static grn_rc
normalize_utf8(grn_ctx *ctx, grn_str *nstr)
{
  int16_t *ch;
  const unsigned char *s, *s_, *s__ = NULL, *p, *p2, *pe, *e;
  unsigned char *d, *d_;
  uint_least8_t *cp, *ctypes;
  size_t length = 0, ls, lp, size = nstr->orig_blen;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size * 5 + 1))) { /* todo: realloc unless enough */
    return GRN_NO_MEMORY_AVAILABLE;
  }
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * 5 * sizeof(int16_t) + 1))) { /* todo: realloc unless enough */
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size * 3 + 1))) { /* todo: realloc unless enough */
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *)nstr->orig,
       d = (unsigned char *)nstr->norm, d_ = NULL; ; s += ls) {
    if (!(ls = grn_str_charlen_utf8(ctx, s, e))) {
      break;
    }
    if ((p = (unsigned char *)grn_nfkc_map1(s))) {
      pe = p + strlen((char *)p);
    } else {
      p = s;
      pe = p + ls;
    }
    if (d_ && (p2 = (unsigned char *)grn_nfkc_map2(d_, p))) {
      p = p2;
      pe = p + strlen((char *)p);
      if (cp) { cp--; }
      if (ch) {
        ch -= (d - d_);
        s_ = s__;
      }
      d = d_;
      length--;
    }
    for (; ; p += lp) {
      if (!(lp = grn_str_charlen_utf8(ctx, p, pe))) {
        break;
      }
      if ((*p == ' ' && removeblankp)
          || *p < 0x20  /* skip unprintable ascii */ ) {
        if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
      } else {
        memcpy(d, p, lp);
        d_ = d;
        d += lp;
        length++;
        if (cp) { *cp++ = grn_nfkc_ctype(p); }
        if (ch) {
          size_t i;
          if (s_ == s + ls) {
            *ch++ = -1;
          } else {
            *ch++ = (int16_t)(s + ls - s_);
            s__ = s_;
            s_ = s + ls;
          }
          for (i = lp; i > 1; i--) { *ch++ = 0; }
        }
      }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}
#endif /* NO_NFKC */

inline static grn_rc
normalize_sjis(grn_ctx *ctx, grn_str *nstr)
{
  static uint16_t hankana[] = {
    0x8140, 0x8142, 0x8175, 0x8176, 0x8141, 0x8145, 0x8392, 0x8340, 0x8342,
    0x8344, 0x8346, 0x8348, 0x8383, 0x8385, 0x8387, 0x8362, 0x815b, 0x8341,
    0x8343, 0x8345, 0x8347, 0x8349, 0x834a, 0x834c, 0x834e, 0x8350, 0x8352,
    0x8354, 0x8356, 0x8358, 0x835a, 0x835c, 0x835e, 0x8360, 0x8363, 0x8365,
    0x8367, 0x8369, 0x836a, 0x836b, 0x836c, 0x836d, 0x836e, 0x8371, 0x8374,
    0x8377, 0x837a, 0x837d, 0x837e, 0x8380, 0x8381, 0x8382, 0x8384, 0x8386,
    0x8388, 0x8389, 0x838a, 0x838b, 0x838c, 0x838d, 0x838f, 0x8393, 0x814a,
    0x814b
  };
  static unsigned char dakuten[] = {
    0x94, 0, 0, 0, 0, 0x4b, 0, 0x4d, 0, 0x4f, 0, 0x51, 0, 0x53, 0, 0x55, 0,
    0x57, 0, 0x59, 0, 0x5b, 0, 0x5d, 0, 0x5f, 0, 0x61, 0, 0, 0x64, 0, 0x66,
    0, 0x68, 0, 0, 0, 0, 0, 0, 0x6f, 0, 0, 0x72, 0, 0, 0x75, 0, 0, 0x78, 0,
    0, 0x7b
  };
  static unsigned char handaku[] = {
    0x70, 0, 0, 0x73, 0, 0, 0x76, 0, 0, 0x79, 0, 0, 0x7c
  };
  int16_t *ch;
  const unsigned char *s, *s_;
  unsigned char *d, *d0, *d_, b, *e;
  uint_least8_t *cp, *ctypes, ctype;
  size_t size = nstr->orig_blen, length = 0;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size * 2 + 1))) {
    return GRN_NO_MEMORY_AVAILABLE;
  }
  d0 = (unsigned char *) nstr->norm;
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * 2 * sizeof(int16_t) + 1))) {
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size + 1))) {
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *) nstr->orig, d = d_ = d0; s < e; s++) {
    if ((*s & 0x80)) {
      if (0xa0 <= *s && *s <= 0xdf) {
        uint16_t c = hankana[*s - 0xa0];
        switch (c) {
        case 0x814a :
          if (d > d0 + 1 && d[-2] == 0x83
              && 0x45 <= d[-1] && d[-1] <= 0x7a && (b = dakuten[d[-1] - 0x45])) {
            *(d - 1) = b;
            if (ch) { ch[-1]++; s_++; }
            continue;
          } else {
            *d++ = c >> 8; *d = c & 0xff;
          }
          break;
        case 0x814b :
          if (d > d0 + 1 && d[-2] == 0x83
              && 0x6e <= d[-1] && d[-1] <= 0x7a && (b = handaku[d[-1] - 0x6e])) {
            *(d - 1) = b;
            if (ch) { ch[-1]++; s_++; }
            continue;
          } else {
            *d++ = c >> 8; *d = c & 0xff;
          }
          break;
        default :
          *d++ = c >> 8; *d = c & 0xff;
          break;
        }
        ctype = grn_str_katakana;
      } else {
        if ((s + 1) < e && 0x40 <= *(s + 1) && *(s + 1) <= 0xfc) {
          unsigned char c1 = *s++, c2 = *s, c3 = 0;
          if (0x81 <= c1 && c1 <= 0x87) {
            switch (c1 & 0x0f) {
            case 1 :
              switch (c2) {
              case 0x5b :
                *d++ = c1; *d = c2;
                ctype = grn_str_katakana;
                break;
              case 0x58 :
                *d++ = c1; *d = c2;
                ctype = grn_str_kanji;
                break;
              case 0x40 :
                if (removeblankp) {
                  if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
                  continue;
                } else {
                  *d = ' ';
                  ctype = GRN_STR_BLANK|grn_str_symbol;
                }
                break;
              default :
                if (0x43 <= c2 && c2 <= 0x7e && (c3 = symbol[c2 - 0x43])) {
                  *d = c3;
                  ctype = grn_str_symbol;
                } else if (0x7f <= c2 && c2 <= 0x97 && (c3 = symbol[c2 - 0x44])) {
                  *d = c3;
                  ctype = grn_str_symbol;
                } else {
                  *d++ = c1; *d = c2;
                  ctype = grn_str_others;
                }
                break;
              }
              break;
            case 2 :
              c3 = c2 - 0x1f;
              if (0x4f <= c2 && c2 <= 0x58) {
                ctype = grn_str_digit;
                *d = c2 - 0x1f;
              } else if (0x60 <= c2 && c2 <= 0x79) {
                ctype = grn_str_alpha;
                *d = c2 + 0x01;
              } else if (0x81 <= c2 && c2 <= 0x9a) {
                ctype = grn_str_alpha;
                *d = c2 - 0x20;
              } else if (0x9f <= c2 && c2 <= 0xf1) {
                *d++ = c1; *d = c2;
                ctype = grn_str_hiragana;
              } else {
                *d++ = c1; *d = c2;
                ctype = grn_str_others;
              }
              break;
            case 3 :
              if (0x40 <= c2 && c2 <= 0x96) {
                *d++ = c1; *d = c2;
                ctype = grn_str_katakana;
              } else {
                *d++ = c1; *d = c2;
                ctype = grn_str_symbol;
              }
              break;
            case 4 :
            case 7 :
              *d++ = c1; *d = c2;
              ctype = grn_str_symbol;
              break;
            default :
              *d++ = c1; *d = c2;
              ctype = grn_str_others;
              break;
            }
          } else {
            *d++ = c1; *d = c2;
            ctype = grn_str_kanji;
          }
        } else {
          /* skip invalid character */
          continue;
        }
      }
    } else {
      unsigned char c = *s;
      switch (c >> 4) {
      case 0 :
      case 1 :
        /* skip unprintable ascii */
        if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
        continue;
      case 2 :
        if (c == 0x20) {
          if (removeblankp) {
            if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
            continue;
          } else {
            *d = ' ';
            ctype = GRN_STR_BLANK|grn_str_symbol;
          }
        } else {
          *d = c;
          ctype = grn_str_symbol;
        }
        break;
      case 3 :
        *d = c;
        ctype = (c <= 0x39) ? grn_str_digit : grn_str_symbol;
        break;
      case 4 :
        *d = ('A' <= c) ? c + 0x20 : c;
        ctype = (c == 0x40) ? grn_str_symbol : grn_str_alpha;
        break;
      case 5 :
        *d = (c <= 'Z') ? c + 0x20 : c;
        ctype = (c <= 0x5a) ? grn_str_alpha : grn_str_symbol;
        break;
      case 6 :
        *d = c;
        ctype = (c == 0x60) ? grn_str_symbol : grn_str_alpha;
        break;
      case 7 :
        *d = c;
        ctype = (c <= 0x7a) ? grn_str_alpha : (c == 0x7f ? grn_str_others : grn_str_symbol);
        break;
      default :
        *d = c;
        ctype = grn_str_others;
        break;
      }
    }
    d++;
    length++;
    if (cp) { *cp++ = ctype; }
    if (ch) {
      *ch++ = (int16_t)(s + 1 - s_);
      s_ = s + 1;
      while (++d_ < d) { *ch++ = 0; }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}

inline static grn_rc
normalize_none(grn_ctx *ctx, grn_str *nstr)
{
  int16_t *ch;
  const unsigned char *s, *s_, *e;
  unsigned char *d, *d0, *d_;
  uint_least8_t *cp, *ctypes, ctype;
  size_t size = nstr->orig_blen, length = 0;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size + 1))) {
    return GRN_NO_MEMORY_AVAILABLE;
  }
  d0 = (unsigned char *) nstr->norm;
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * sizeof(int16_t) + 1))) {
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size + 1))) {
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *) nstr->orig, d = d_ = d0; s < e; s++) {
    unsigned char c = *s;
    switch (c >> 4) {
    case 0 :
    case 1 :
      /* skip unprintable ascii */
      if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
      continue;
    case 2 :
      if (c == 0x20) {
        if (removeblankp) {
          if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
          continue;
        } else {
          *d = ' ';
          ctype = GRN_STR_BLANK|grn_str_symbol;
        }
      } else {
        *d = c;
        ctype = grn_str_symbol;
      }
      break;
    case 3 :
      *d = c;
      ctype = (c <= 0x39) ? grn_str_digit : grn_str_symbol;
      break;
    case 4 :
      *d = ('A' <= c) ? c + 0x20 : c;
      ctype = (c == 0x40) ? grn_str_symbol : grn_str_alpha;
      break;
    case 5 :
      *d = (c <= 'Z') ? c + 0x20 : c;
      ctype = (c <= 0x5a) ? grn_str_alpha : grn_str_symbol;
      break;
    case 6 :
      *d = c;
      ctype = (c == 0x60) ? grn_str_symbol : grn_str_alpha;
      break;
    case 7 :
      *d = c;
      ctype = (c <= 0x7a) ? grn_str_alpha : (c == 0x7f ? grn_str_others : grn_str_symbol);
      break;
    default :
      *d = c;
      ctype = grn_str_others;
      break;
    }
    d++;
    length++;
    if (cp) { *cp++ = ctype; }
    if (ch) {
      *ch++ = (int16_t)(s + 1 - s_);
      s_ = s + 1;
      while (++d_ < d) { *ch++ = 0; }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}

/* use cp1252 as latin1 */
inline static grn_rc
normalize_latin1(grn_ctx *ctx, grn_str *nstr)
{
  int16_t *ch;
  const unsigned char *s, *s_, *e;
  unsigned char *d, *d0, *d_;
  uint_least8_t *cp, *ctypes, ctype;
  size_t size = strlen(nstr->orig), length = 0;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size + 1))) {
    return GRN_NO_MEMORY_AVAILABLE;
  }
  d0 = (unsigned char *) nstr->norm;
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * sizeof(int16_t) + 1))) {
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size + 1))) {
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *) nstr->orig, d = d_ = d0; s < e; s++) {
    unsigned char c = *s;
    switch (c >> 4) {
    case 0 :
    case 1 :
      /* skip unprintable ascii */
      if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
      continue;
    case 2 :
      if (c == 0x20) {
        if (removeblankp) {
          if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
          continue;
        } else {
          *d = ' ';
          ctype = GRN_STR_BLANK|grn_str_symbol;
        }
      } else {
        *d = c;
        ctype = grn_str_symbol;
      }
      break;
    case 3 :
      *d = c;
      ctype = (c <= 0x39) ? grn_str_digit : grn_str_symbol;
      break;
    case 4 :
      *d = ('A' <= c) ? c + 0x20 : c;
      ctype = (c == 0x40) ? grn_str_symbol : grn_str_alpha;
      break;
    case 5 :
      *d = (c <= 'Z') ? c + 0x20 : c;
      ctype = (c <= 0x5a) ? grn_str_alpha : grn_str_symbol;
      break;
    case 6 :
      *d = c;
      ctype = (c == 0x60) ? grn_str_symbol : grn_str_alpha;
      break;
    case 7 :
      *d = c;
      ctype = (c <= 0x7a) ? grn_str_alpha : (c == 0x7f ? grn_str_others : grn_str_symbol);
      break;
    case 8 :
      if (c == 0x8a || c == 0x8c || c == 0x8e) {
        *d = c + 0x10;
        ctype = grn_str_alpha;
      } else {
        *d = c;
        ctype = grn_str_symbol;
      }
      break;
    case 9 :
      if (c == 0x9a || c == 0x9c || c == 0x9e || c == 0x9f) {
        *d = (c == 0x9f) ? c + 0x60 : c;
        ctype = grn_str_alpha;
      } else {
        *d = c;
        ctype = grn_str_symbol;
      }
      break;
    case 0x0c :
      *d = c + 0x20;
      ctype = grn_str_alpha;
      break;
    case 0x0d :
      *d = (c == 0xd7 || c == 0xdf) ? c : c + 0x20;
      ctype = (c == 0xd7) ? grn_str_symbol : grn_str_alpha;
      break;
    case 0x0e :
      *d = c;
      ctype = grn_str_alpha;
      break;
    case 0x0f :
      *d = c;
      ctype = (c == 0xf7) ? grn_str_symbol : grn_str_alpha;
      break;
    default :
      *d = c;
      ctype = grn_str_others;
      break;
    }
    d++;
    length++;
    if (cp) { *cp++ = ctype; }
    if (ch) {
      *ch++ = (int16_t)(s + 1 - s_);
      s_ = s + 1;
      while (++d_ < d) { *ch++ = 0; }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}

inline static grn_rc
normalize_koi8r(grn_ctx *ctx, grn_str *nstr)
{
  int16_t *ch;
  const unsigned char *s, *s_, *e;
  unsigned char *d, *d0, *d_;
  uint_least8_t *cp, *ctypes, ctype;
  size_t size = strlen(nstr->orig), length = 0;
  int removeblankp = nstr->flags & GRN_STR_REMOVEBLANK;
  if (!(nstr->norm = GRN_MALLOC(size + 1))) {
    return GRN_NO_MEMORY_AVAILABLE;
  }
  d0 = (unsigned char *) nstr->norm;
  if (nstr->flags & GRN_STR_WITH_CHECKS) {
    if (!(nstr->checks = GRN_MALLOC(size * sizeof(int16_t) + 1))) {
      GRN_FREE(nstr->norm);
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  ch = nstr->checks;
  if (nstr->flags & GRN_STR_WITH_CTYPES) {
    if (!(nstr->ctypes = GRN_MALLOC(size + 1))) {
      GRN_FREE(nstr->checks);
      GRN_FREE(nstr->norm);
      nstr->checks = NULL;
      nstr->norm = NULL;
      return GRN_NO_MEMORY_AVAILABLE;
    }
  }
  cp = ctypes = nstr->ctypes;
  e = (unsigned char *)nstr->orig + size;
  for (s = s_ = (unsigned char *) nstr->orig, d = d_ = d0; s < e; s++) {
    unsigned char c = *s;
    switch (c >> 4) {
    case 0 :
    case 1 :
      /* skip unprintable ascii */
      if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
      continue;
    case 2 :
      if (c == 0x20) {
        if (removeblankp) {
          if (cp > ctypes) { *(cp - 1) |= GRN_STR_BLANK; }
          continue;
        } else {
          *d = ' ';
          ctype = GRN_STR_BLANK|grn_str_symbol;
        }
      } else {
        *d = c;
        ctype = grn_str_symbol;
      }
      break;
    case 3 :
      *d = c;
      ctype = (c <= 0x39) ? grn_str_digit : grn_str_symbol;
      break;
    case 4 :
      *d = ('A' <= c) ? c + 0x20 : c;
      ctype = (c == 0x40) ? grn_str_symbol : grn_str_alpha;
      break;
    case 5 :
      *d = (c <= 'Z') ? c + 0x20 : c;
      ctype = (c <= 0x5a) ? grn_str_alpha : grn_str_symbol;
      break;
    case 6 :
      *d = c;
      ctype = (c == 0x60) ? grn_str_symbol : grn_str_alpha;
      break;
    case 7 :
      *d = c;
      ctype = (c <= 0x7a) ? grn_str_alpha : (c == 0x7f ? grn_str_others : grn_str_symbol);
      break;
    case 0x0a :
      *d = c;
      ctype = (c == 0xa3) ? grn_str_alpha : grn_str_others;
      break;
    case 0x0b :
      if (c == 0xb3) {
        *d = c - 0x10;
        ctype = grn_str_alpha;
      } else {
        *d = c;
        ctype = grn_str_others;
      }
      break;
    case 0x0c :
    case 0x0d :
      *d = c;
      ctype = grn_str_alpha;
      break;
    case 0x0e :
    case 0x0f :
      *d = c - 0x20;
      ctype = grn_str_alpha;
      break;
    default :
      *d = c;
      ctype = grn_str_others;
      break;
    }
    d++;
    length++;
    if (cp) { *cp++ = ctype; }
    if (ch) {
      *ch++ = (int16_t)(s + 1 - s_);
      s_ = s + 1;
      while (++d_ < d) { *ch++ = 0; }
    }
  }
  if (cp) { *cp = grn_str_null; }
  *d = '\0';
  nstr->length = length;
  nstr->norm_blen = (size_t)(d - (unsigned char *)nstr->norm);
  return GRN_SUCCESS;
}

static grn_str *
grn_fakenstr_open(grn_ctx *ctx, const char *str, size_t str_len, grn_encoding encoding, int flags)
{
  /* TODO: support GRN_STR_REMOVEBLANK flag and ctypes */
  grn_str *nstr;
  if (!(nstr = GRN_MALLOC(sizeof(grn_str)))) {
    GRN_LOG(grn_log_alert, "memory allocation on grn_fakenstr_open failed !");
    return NULL;
  }
  if (!(nstr->norm = GRN_MALLOC(str_len + 1))) {
    GRN_LOG(grn_log_alert, "memory allocation for keyword on grn_snip_add_cond failed !");
    GRN_FREE(nstr);
    return NULL;
  }
  nstr->orig = str;
  nstr->orig_blen = str_len;
  memcpy(nstr->norm, str, str_len);
  nstr->norm[str_len] = '\0';
  nstr->norm_blen = str_len;
  nstr->ctypes = NULL;
  nstr->flags = flags;

  if (flags & GRN_STR_WITH_CHECKS) {
    int16_t f = 0;
    unsigned char c;
    size_t i;
    if (!(nstr->checks = (int16_t *) GRN_MALLOC(sizeof(int16_t) * str_len))) {
      GRN_FREE(nstr->norm);
      GRN_FREE(nstr);
      return NULL;
    }
    switch (encoding) {
    case grn_enc_euc_jp:
      for (i = 0; i < str_len; i++) {
        if (!f) {
          c = (unsigned char) str[i];
          f = ((c >= 0xa1U && c <= 0xfeU) || c == 0x8eU ? 2 : (c == 0x8fU ? 3 : 1)
            );
          nstr->checks[i] = f;
        } else {
          nstr->checks[i] = 0;
        }
        f--;
      }
      break;
    case grn_enc_sjis:
      for (i = 0; i < str_len; i++) {
        if (!f) {
          c = (unsigned char) str[i];
          f = (c >= 0x81U && ((c <= 0x9fU) || (c >= 0xe0U && c <= 0xfcU)) ? 2 : 1);
          nstr->checks[i] = f;
        } else {
          nstr->checks[i] = 0;
        }
        f--;
      }
      break;
    case grn_enc_utf8:
      for (i = 0; i < str_len; i++) {
        if (!f) {
          c = (unsigned char) str[i];
          f = (c & 0x80U ? (c & 0x20U ? (c & 0x10U ? 4 : 3)
                           : 2)
               : 1);
          nstr->checks[i] = f;
        } else {
          nstr->checks[i] = 0;
        }
        f--;
      }
      break;
    default:
      for (i = 0; i < str_len; i++) {
        nstr->checks[i] = 1;
      }
      break;
    }
  } else {
    nstr->checks = NULL;
  }
  return nstr;
}

grn_str *
grn_str_open(grn_ctx *ctx, const char *str, unsigned int str_len,
             grn_encoding encoding, int flags)
{
  grn_rc rc;
  grn_str *nstr;
  if (!str) { return NULL; }

  if (!(flags & GRN_STR_NORMALIZE)) {
    return grn_fakenstr_open(ctx, str, str_len, encoding, flags);
  }

  if (!(nstr = GRN_MALLOC(sizeof(grn_str)))) {
    GRN_LOG(grn_log_alert, "memory allocation on grn_str_open failed !");
    return NULL;
  }
  nstr->orig = str;
  nstr->orig_blen = str_len;
  nstr->norm = NULL;
  nstr->norm_blen = 0;
  nstr->checks = NULL;
  nstr->ctypes = NULL;
  nstr->encoding = encoding;
  nstr->flags = flags;
  switch (encoding) {
  case grn_enc_euc_jp :
    rc = normalize_euc(ctx, nstr);
    break;
  case grn_enc_utf8 :
#ifdef NO_NFKC
    rc = normalize_none(ctx, nstr);
#else /* NO_NFKC */
    rc = normalize_utf8(ctx, nstr);
#endif /* NO_NFKC */
    break;
  case grn_enc_sjis :
    rc = normalize_sjis(ctx, nstr);
    break;
  case grn_enc_latin1 :
    rc = normalize_latin1(ctx, nstr);
    break;
  case grn_enc_koi8r :
    rc = normalize_koi8r(ctx, nstr);
    break;
  default :
    rc = normalize_none(ctx, nstr);
    break;
  }
  if (rc) {
    grn_str_close(ctx, nstr);
    return NULL;
  }
  return nstr;
}

grn_rc
grn_str_close(grn_ctx *ctx, grn_str *nstr)
{
  if (nstr) {
    if (nstr->norm) { GRN_FREE(nstr->norm); }
    if (nstr->ctypes) { GRN_FREE(nstr->ctypes); }
    if (nstr->checks) { GRN_FREE(nstr->checks); }
    GRN_FREE(nstr);
    return GRN_SUCCESS;
  } else {
    return GRN_INVALID_ARGUMENT;
  }
}

static const char *grn_enc_string[] = {
  "default",
  "none",
  "euc_jp",
  "utf8",
  "sjis",
  "latin1",
  "koi8r"
};

const char *
grn_enctostr(grn_encoding enc)
{
  if (enc < (sizeof(grn_enc_string) / sizeof(char *))) {
    return grn_enc_string[enc];
  } else {
    return "unknown";
  }
}

grn_encoding
grn_strtoenc(const char *str)
{
  grn_encoding e = grn_enc_euc_jp;
  int i = sizeof(grn_enc_string) / sizeof(grn_enc_string[0]);
  while (i--) {
    if (!strcmp(str, grn_enc_string[i])) {
      e = (grn_encoding)i;
    }
  }
  return e;
}

size_t
grn_str_len(grn_ctx *ctx, const char *str, grn_encoding encoding, const char **last)
{
  size_t len, tlen;
  const char *p = NULL;
  for (len = 0; ; len++) {
    p = str;
    if (!(tlen = grn_str_charlen(ctx, str, encoding))) {
      break;
    }
    str += tlen;
  }
  if (last) { *last = p; }
  return len;
}

int
grn_isspace(const char *str, grn_encoding encoding)
{
  const unsigned char *s = (const unsigned char *) str;
  if (!s) { return 0; }
  switch (s[0]) {
  case ' ' :
  case '\f' :
  case '\n' :
  case '\r' :
  case '\t' :
  case '\v' :
    return 1;
  case 0x81 :
    if (encoding == grn_enc_sjis && s[1] == 0x40) { return 2; }
    break;
  case 0xA1 :
    if (encoding == grn_enc_euc_jp && s[1] == 0xA1) { return 2; }
    break;
  case 0xE3 :
    if (encoding == grn_enc_utf8 && s[1] == 0x80 && s[2] == 0x80) { return 3; }
    break;
  default :
    break;
  }
  return 0;
}

int
grn_atoi(const char *nptr, const char *end, const char **rest)
{
  const char *p = nptr;
  int v = 0, t, n = 0, o = 0;
  if (p < end && *p == '-') {
    p++;
    n = 1;
    o = 1;
  }
  while (p < end && *p >= '0' && *p <= '9') {
    t = v * 10 - (*p - '0');
    if (t > v || (!n && t == INT32_MIN)) { v = 0; break; }
    v = t;
    o = 0;
    p++;
  }
  if (rest) { *rest = o ? nptr : p; }
  return n ? v : -v;
}

unsigned int
grn_atoui(const char *nptr, const char *end, const char **rest)
{
  unsigned int v = 0, t;
  while (nptr < end && *nptr >= '0' && *nptr <= '9') {
    t = v * 10 + (*nptr - '0');
    if (t < v) { v = 0; break; }
    v = t;
    nptr++;
  }
  if (rest) { *rest = nptr; }
  return v;
}

int64_t
grn_atoll(const char *nptr, const char *end, const char **rest)
{
  /* FIXME: INT_MIN is not supported */
  const char *p = nptr;
  int n = 0, o = 0;
  int64_t v = 0, t;
  if (p < end && *p == '-') {
    p++;
    n = 1;
    o = 1;
  }
  while (p < end && *p >= '0' && *p <= '9') {
    t = v * 10 + (*p - '0');
    if (t < v) { v = 0; break; }
    v = t;
    o = 0;
    p++;
  }
  if (rest) { *rest = o ? nptr : p; }
  return n ? -v : v;
}

unsigned int
grn_htoui(const char *nptr, const char *end, const char **rest)
{
  unsigned int v = 0, t;
  while (nptr < end) {
    switch (*nptr) {
    case '0' :
    case '1' :
    case '2' :
    case '3' :
    case '4' :
    case '5' :
    case '6' :
    case '7' :
    case '8' :
    case '9' :
      t = v * 16 + (*nptr++ - '0');
      break;
    case 'a' :
    case 'b' :
    case 'c' :
    case 'd' :
    case 'e' :
    case 'f' :
      t = v * 16 + (*nptr++ - 'a') + 10;
      break;
    case 'A' :
    case 'B' :
    case 'C' :
    case 'D' :
    case 'E' :
    case 'F' :
      t = v * 16 + (*nptr++ - 'A') + 10;
      break;
    default :
      v = 0; goto exit;
    }
    if (t < v) { v = 0; goto exit; }
    v = t;
  }
exit :
  if (rest) { *rest = nptr; }
  return v;
}

void
grn_itoh(unsigned int i, char *p, unsigned int len)
{
  static const char *hex = "0123456789ABCDEF";
  p += len;
  *p-- = '\0';
  while (len--) {
    *p-- = hex[i & 0xf];
    i >>= 4;
  }
}

grn_rc
grn_itoa(int i, char *p, char *end, char **rest)
{
  char *q;
  if (p >= end) { return GRN_INVALID_ARGUMENT; }
  q = p;
  if (i < 0) {
    *p++ = '-';
    q = p;
    if (i == INT_MIN) {
      *p++ = (-(i % 10)) + '0';
      i /= 10;
    }
    i = -i;
  }
  do {
    if (p >= end) { return GRN_INVALID_ARGUMENT; }
    *p++ = i % 10 + '0';
  } while ((i /= 10) > 0);
  if (rest) { *rest = p; }
  for (p--; q < p; q++, p--) {
    char t = *q;
    *q = *p;
    *p = t;
  }
  return GRN_SUCCESS;
}

grn_rc
grn_lltoa(int64_t i, char *p, char *end, char **rest)
{
  char *q;
  if (p >= end) { return GRN_INVALID_ARGUMENT; }
  q = p;
  if (i < 0) {
    *p++ = '-';
    q = p;
    if (i == INT64_MIN) {
      *p++ = (-(i % 10)) + '0';
      i /= 10;
    }
    i = -i;
  }
  do {
    if (p >= end) { return GRN_INVALID_ARGUMENT; }
    *p++ = i % 10 + '0';
  } while ((i /= 10) > 0);
  if (rest) { *rest = p; }
  for (p--; q < p; q++, p--) {
    char t = *q;
    *q = *p;
    *p = t;
  }
  return GRN_SUCCESS;
}

#define I2B(i) \
 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(i) & 0x3f])

#define B2I(b) \
 (((b) < '+' || 'z' < (b)) ? 0xff : "\x3e\xff\xff\xff\x3f\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xff\xff\xff\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33"[(b) - '+'])

#define MASK 0x34d34d34

char *
grn_itob(grn_id id, char *p)
{
  id ^= MASK;
  *p++ = I2B(id >> 24);
  *p++ = I2B(id >> 18);
  *p++ = I2B(id >> 12);
  *p++ = I2B(id >> 6);
  *p++ = I2B(id);
  return p;
}

grn_id
grn_btoi(char *b)
{
  uint8_t i;
  grn_id id = 0;
  int len = 5;
  while (len--) {
    char c = *b++;
    if ((i = B2I(c)) == 0xff) { return 0; }
    id = (id << 6) + i;
  }
  return id ^ MASK;
}

#define I2B32H(i) ("0123456789ABCDEFGHIJKLMNOPQRSTUV"[(i) & 0x1f])

char *
grn_lltob32h(int64_t i, char *p)
{
  uint64_t u = (uint64_t)i + 0x8000000000000000ULL;
  *p++ = I2B32H(u >> 60);
  *p++ = I2B32H(u >> 55);
  *p++ = I2B32H(u >> 50);
  *p++ = I2B32H(u >> 45);
  *p++ = I2B32H(u >> 40);
  *p++ = I2B32H(u >> 35);
  *p++ = I2B32H(u >> 30);
  *p++ = I2B32H(u >> 25);
  *p++ = I2B32H(u >> 20);
  *p++ = I2B32H(u >> 15);
  *p++ = I2B32H(u >> 10);
  *p++ = I2B32H(u >> 5);
  *p++ = I2B32H(u);
  return p;
}

char *
grn_ulltob32h(uint64_t i, char *p)
{
  char lb = (i >> 59) & 0x10;
  i += 0x8000000000000000ULL;
  *p++ = lb + I2B32H(i >> 60);
  *p++ = I2B32H(i >> 55);
  *p++ = I2B32H(i >> 50);
  *p++ = I2B32H(i >> 45);
  *p++ = I2B32H(i >> 40);
  *p++ = I2B32H(i >> 35);
  *p++ = I2B32H(i >> 30);
  *p++ = I2B32H(i >> 25);
  *p++ = I2B32H(i >> 20);
  *p++ = I2B32H(i >> 15);
  *p++ = I2B32H(i >> 10);
  *p++ = I2B32H(i >> 5);
  *p++ = I2B32H(i);
  return p;
}

int
grn_str_tok(char *str, size_t str_len, char delim, char **tokbuf, int buf_size, char **rest)
{
  char **tok = tokbuf, **tok_end = tokbuf + buf_size;
  if (buf_size > 0) {
    char *str_end = str + str_len;
    for (;;str++) {
      if (str == str_end) {
        *tok++ = str;
        break;
      }
      if (delim == *str) {
        *str = '\0';
        *tok++ = str;
        if (tok == tok_end) { break; }
      }
    }
  }
  if (rest) { *rest = str; }
  return tok - tokbuf;
}

inline static void
op_getopt_flag(int *flags, const grn_str_getopt_opt *o,
               int argc, char * const argv[], int *i)
{
  switch (o->op) {
    case getopt_op_none:
      break;
    case getopt_op_on:
      *flags |= o->flag;
      break;
    case getopt_op_off:
      *flags &= ~o->flag;
      break;
    case getopt_op_update:
      *flags = o->flag;
      break;
    default:
      return;
  }
  if (o->arg) {
    if (++(*i) < argc) {
      *o->arg = argv[*i];
    } else {
      /* TODO: error */
    }
  }
}

int
grn_str_getopt(int argc, char * const argv[], const grn_str_getopt_opt *opts,
               int *flags)
{
  int i;
  for (i = 1; i < argc; i++) {
    const char * v = argv[i];
    if (*v == '-') {
      const grn_str_getopt_opt *o;
      int found;
      if (*++v == '-') {
        found = 0;
        for (o = opts; o->opt != '\0' || o->longopt != NULL; o++) {
          if (o->longopt && !strcmp(v, o->longopt)) {
            op_getopt_flag(flags, o, argc, argv, &i);
            found = 1;
            break;
          }
        }
        if (!found) { goto exit; }
      } else {
        const char *p;
        for (p = v; *p; p++) {
          found = 0;
          for (o = opts; o->opt != '\0' || o->longopt != NULL; o++) {
            if (o->opt && *p == o->opt) {
              op_getopt_flag(flags, o, argc, argv, &i);
              found = 1;
              break;
            }
          }
          if (!found) { goto exit; }
        }
      }
    } else {
      break;
    }
  }
  return i;
exit:
  fprintf(stderr, "cannot recognize option '%s'.\n", argv[i]);
  return -1;
}

#define UNIT_SIZE (1 << 12)
#define UNIT_MASK (UNIT_SIZE - 1)

int grn_bulk_margin_size = 0;

grn_rc
grn_bulk_init(grn_ctx *ctx, grn_obj *buf, unsigned int size)
{
  buf->u.b.head = NULL;
  buf->u.b.curr = NULL;
  buf->u.b.tail = NULL;
  return size ? grn_bulk_resize(ctx, buf, size) : GRN_SUCCESS;
}

grn_rc
grn_bulk_resize(grn_ctx *ctx, grn_obj *buf, unsigned int newsize)
{
  char *head;
  newsize += grn_bulk_margin_size + 1;
  newsize = (newsize + (UNIT_MASK)) & ~UNIT_MASK;
  head = buf->u.b.head - (buf->u.b.head ? grn_bulk_margin_size : 0);
  if (!(head = GRN_REALLOC(head, newsize))) { return GRN_NO_MEMORY_AVAILABLE; }
  buf->u.b.curr = head + grn_bulk_margin_size + GRN_BULK_VSIZE(buf);
  buf->u.b.head = head + grn_bulk_margin_size;
  buf->u.b.tail = head + newsize;
  return GRN_SUCCESS;
}

grn_rc
grn_bulk_reinit(grn_ctx *ctx, grn_obj *buf, unsigned int size)
{
  GRN_BULK_REWIND(buf);
  return grn_bulk_resize(ctx, buf, size);
}

grn_rc
grn_bulk_write(grn_ctx *ctx, grn_obj *buf, const char *str, unsigned int len)
{
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < len) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + len))) { return rc; }
  }
  memcpy(buf->u.b.curr, str, len);
  buf->u.b.curr += len;
  return rc;
}

grn_rc
grn_bulk_reserve(grn_ctx *ctx, grn_obj *buf, unsigned int len)
{
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < len) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + len))) { return rc; }
  }
  return rc;
}

grn_rc
grn_bulk_space(grn_ctx *ctx, grn_obj *buf, unsigned int len)
{
  grn_rc rc = grn_bulk_reserve(ctx, buf, len);
  if (!rc) { buf->u.b.curr += len; }
  return rc;
}

grn_rc
grn_bulk_itoa(grn_ctx *ctx, grn_obj *buf, int i)
{
  grn_rc rc = GRN_SUCCESS;
  while (grn_itoa(i, buf->u.b.curr, buf->u.b.tail, &buf->u.b.curr)) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_WSIZE(buf) + UNIT_SIZE))) { return rc; }
  }
  return rc;
}

grn_rc
grn_bulk_lltoa(grn_ctx *ctx, grn_obj *buf, long long int i)
{
  grn_rc rc = GRN_SUCCESS;
  while (grn_lltoa(i, buf->u.b.curr, buf->u.b.tail, &buf->u.b.curr)) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_WSIZE(buf) + UNIT_SIZE))) { return rc; }
  }
  return rc;
}

inline static void
ftoa_(grn_ctx *ctx, grn_obj *buf, double d)
{
  size_t len = sprintf(buf->u.b.curr, "%#.15g", d);
  if (buf->u.b.curr[len - 1] == '.') {
    buf->u.b.curr += len;
    GRN_BULK_PUTC(ctx, buf, '0');
  } else {
    char *p, *q;
    buf->u.b.curr[len] = '\0';
    if ((p = strchr(buf->u.b.curr, 'e'))) {
      for (q = p; *(q - 2) != '.' && *(q - 1) == '0'; q--) { len--; }
      memmove(q, p, buf->u.b.curr + len - q);
    } else {
      for (q = buf->u.b.curr + len; *(q - 2) != '.' && *(q - 1) == '0'; q--) { len--; }
    }
    buf->u.b.curr += len;
  }
}

grn_rc
grn_bulk_ftoa(grn_ctx *ctx, grn_obj *buf, double d)
{
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < 32) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + 32))) { return rc; }
  }
#ifdef HAVE_FPCLASSIFY
  switch (fpclassify(d)) {
  CASE_FP_NAN
    GRN_BULK_PUTS(ctx, buf, "#<nan>");
    break;
  CASE_FP_INFINITE
    GRN_BULK_PUTS(ctx, buf, d > 0 ? "#i1/0" : "#i-1/0");
    break;
  default :
    ftoa_(ctx, buf, d);
    break;
  }
#else /* HAVE_FPCLASSIFY */
  if (d == d) {
    if (d != 0 && ((d / 2.0) == d)) {
      GRN_BULK_PUTS(ctx, buf, d > 0 ? "#i1/0" : "#i-1/0");
    } else {
      ftoa_(ctx, buf, d);
    }
  } else {
    GRN_BULK_PUTS(ctx, buf, "#<nan>");
  }
#endif /* HAVE_FPCLASSIFY */
  return rc;
}

grn_rc
grn_bulk_itoh(grn_ctx *ctx, grn_obj *buf, int i, unsigned int len)
{
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < len) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + len))) { return rc; }
  }
  grn_itoh(i, buf->u.b.curr, len);
  buf->u.b.curr += len;
  return rc;
}

grn_rc
grn_bulk_itob(grn_ctx *ctx, grn_obj *buf, grn_id id)
{
  size_t len = 5;
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < len) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + len))) { return rc; }
  }
  grn_itob(id, buf->u.b.curr);
  buf->u.b.curr += len;
  return rc;
}

grn_rc
grn_bulk_lltob32h(grn_ctx *ctx, grn_obj *buf, long long int i)
{
  size_t len = 13;
  grn_rc rc = GRN_SUCCESS;
  if (GRN_BULK_REST(buf) < len) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + len))) { return rc; }
  }
  grn_lltob32h(i, buf->u.b.curr);
  buf->u.b.curr += len;
  return rc;
}

grn_rc
grn_bulk_esc(grn_ctx *ctx, grn_obj *buf, const char *s, unsigned int len, grn_encoding encoding)
{
  const char *e;
  unsigned int l;
  grn_rc rc = GRN_SUCCESS;

  GRN_BULK_PUTC(ctx, buf, '"');
  for (e = s + len; s < e; s += l) {
    if (!(l = grn_charlen(ctx, s, e, encoding))) { break; }
    if (l == 1) {
      switch (*s) {
      case '"' :
        grn_bulk_write(ctx, buf, "\\\"", 2);
        break;
      case '\\' :
        grn_bulk_write(ctx, buf, "\\\\", 2);
        break;
      case '\b' :
        grn_bulk_write(ctx, buf, "\\b", 2);
        break;
      case '\f' :
        grn_bulk_write(ctx, buf, "\\f", 2);
        break;
      case '\n' :
        grn_bulk_write(ctx, buf, "\\n", 2);
        break;
      case '\r' :
        grn_bulk_write(ctx, buf, "\\r", 2);
        break;
      case '\t' :
        grn_bulk_write(ctx, buf, "\\t", 2);
        break;
      case '\x00': case '\x01': case '\x02': case '\x03': case '\x04': case '\x05':
      case '\x06': case '\x07': case '\x0b': case '\x0e': case '\x0f': case '\x10':
      case '\x11': case '\x12': case '\x13': case '\x14': case '\x15': case '\x16':
      case '\x17': case '\x18': case '\x19': case '\x1a': case '\x1b': case '\x1c':
      case '\x1d': case '\x1e': case '\x1f': case '\x7f':
        if (!(rc = grn_bulk_write(ctx, buf, "\\u", 2))) {
          if ((rc = grn_bulk_itoh(ctx, buf, *s, 4))) {
            buf->u.b.curr -= 2;
            return rc;
          }
        } else {
          return rc;
        }
        break;
      default :
        GRN_BULK_PUTC(ctx, buf, *s);
      }
    } else {
      grn_bulk_write(ctx, buf, s, l);
    }
  }
  GRN_BULK_PUTC(ctx, buf, '"');
  return rc;
}

grn_rc
grn_bulk_benc(grn_ctx *ctx, grn_obj *buf, unsigned int v)
{
  grn_rc rc = GRN_SUCCESS;
  uint8_t *p;
  if (GRN_BULK_REST(buf) < 5) {
    if ((rc = grn_bulk_resize(ctx, buf, GRN_BULK_VSIZE(buf) + 5))) { return rc; }
  }
  p = (uint8_t *)buf->u.b.curr;
  GRN_B_ENC(v, p);
  buf->u.b.curr = (char *)p;
  return rc;
}

/* 0x00 - 0x7f */
static const int_least8_t urlenc_tbl[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

grn_rc
grn_bulk_urlenc(grn_ctx *ctx, grn_obj *buf, const char *s, unsigned int len)
{
  const char *e, c = '%';
  for (e = s + len; s < e; s++) {
    if (*s < 0 || urlenc_tbl[(int)*s]) {
      if (!grn_bulk_write(ctx, buf, &c, 1)) {
        if (grn_bulk_itoh(ctx, buf, *s, 2)) {
          buf->u.b.curr--;
        }
      }
    } else {
      GRN_BULK_PUTC(ctx, buf, *s);
    }
  }
  return GRN_SUCCESS;
}

grn_rc
grn_bulk_fin(grn_ctx *ctx, grn_obj *buf)
{
  if (buf->u.b.head) {
    GRN_REALLOC(buf->u.b.head - grn_bulk_margin_size, 0);
    buf->u.b.head = NULL;
  }
  return GRN_SUCCESS;
}

grn_rc
grn_substring(grn_ctx *ctx, char **str, char **str_end, int start, int end, grn_encoding encoding)
{
  int i;
  size_t l;
  char *s = *str, *e = *str_end;
  for (i = 0; s < e; i++, s += l) {
    if (i == start) { *str = s; }
    if (!(l = grn_charlen(ctx, s, e, encoding))) {
      return GRN_INVALID_ARGUMENT;
    }
    if (i == end) {
      *str_end = s;
      break;
    }
  }
  return GRN_SUCCESS;
}