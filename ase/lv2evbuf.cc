// Copyright 2008-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "lv2evbuf.hh"

// for Anklang
#define USE_POSIX_MEMALIGN 1
// #include "jalv_config.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct LV2_Evbuf_Impl {
  uint32_t          capacity;
  uint32_t          atom_Chunk;
  uint32_t          atom_Sequence;
  uint32_t          pad; // So buf has correct atom alignment
  LV2_Atom_Sequence buf;
};

LV2_Evbuf*
lv2_evbuf_new(uint32_t capacity, uint32_t atom_Chunk, uint32_t atom_Sequence)
{
  const size_t buffer_size =
    sizeof(LV2_Evbuf) + sizeof(LV2_Atom_Sequence) + capacity;

#if USE_POSIX_MEMALIGN
  LV2_Evbuf* evbuf = NULL;
  const int  st    = posix_memalign((void**)&evbuf, 16, buffer_size);
  if (st) {
    return NULL;
  }

  assert((uintptr_t)evbuf % 8U == 0U);
#else
  LV2_Evbuf* evbuf = (LV2_Evbuf*)malloc(buffer_size);
#endif

  if (evbuf) {
    memset(evbuf, 0, sizeof(*evbuf));
    evbuf->capacity      = capacity;
    evbuf->atom_Chunk    = atom_Chunk;
    evbuf->atom_Sequence = atom_Sequence;
  }

  return evbuf;
}

void
lv2_evbuf_free(LV2_Evbuf* evbuf)
{
  free(evbuf);
}

void
lv2_evbuf_reset(LV2_Evbuf* evbuf, bool input)
{
  if (input) {
    evbuf->buf.atom.size = sizeof(LV2_Atom_Sequence_Body);
    evbuf->buf.atom.type = evbuf->atom_Sequence;
  } else {
    evbuf->buf.atom.size = evbuf->capacity;
    evbuf->buf.atom.type = evbuf->atom_Chunk;
  }
}

uint32_t
lv2_evbuf_get_size(LV2_Evbuf* evbuf)
{
  assert(evbuf->buf.atom.type != evbuf->atom_Sequence ||
         evbuf->buf.atom.size >= sizeof(LV2_Atom_Sequence_Body));
  return evbuf->buf.atom.type == evbuf->atom_Sequence
           ? evbuf->buf.atom.size - sizeof(LV2_Atom_Sequence_Body)
           : 0;
}

void*
lv2_evbuf_get_buffer(LV2_Evbuf* evbuf)
{
  return &evbuf->buf;
}

LV2_Evbuf_Iterator
lv2_evbuf_begin(LV2_Evbuf* evbuf)
{
  LV2_Evbuf_Iterator iter = {evbuf, 0};
  return iter;
}

LV2_Evbuf_Iterator
lv2_evbuf_end(LV2_Evbuf* evbuf)
{
  const uint32_t           size = lv2_evbuf_get_size(evbuf);
  const LV2_Evbuf_Iterator iter = {evbuf, lv2_atom_pad_size(size)};
  return iter;
}

bool
lv2_evbuf_is_valid(LV2_Evbuf_Iterator iter)
{
  return iter.offset < lv2_evbuf_get_size(iter.evbuf);
}

LV2_Evbuf_Iterator
lv2_evbuf_next(const LV2_Evbuf_Iterator iter)
{
  if (!lv2_evbuf_is_valid(iter)) {
    return iter;
  }

  LV2_Atom_Sequence* aseq = &iter.evbuf->buf;
  LV2_Atom_Event*    aev =
    (LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) +
                      iter.offset);

  const uint32_t offset =
    iter.offset + lv2_atom_pad_size(sizeof(LV2_Atom_Event) + aev->body.size);

  LV2_Evbuf_Iterator next = {iter.evbuf, offset};
  return next;
}

bool
lv2_evbuf_get(LV2_Evbuf_Iterator iter,
              uint32_t*          frames,
              uint32_t*          subframes,
              uint32_t*          type,
              uint32_t*          size,
              void**             data)
{
  *frames = *subframes = *type = *size = 0;
  *data                                = NULL;

  if (!lv2_evbuf_is_valid(iter)) {
    return false;
  }

  LV2_Atom_Sequence* aseq = &iter.evbuf->buf;
  LV2_Atom_Event*    aev =
    (LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) +
                      iter.offset);

  *frames    = aev->time.frames;
  *subframes = 0;
  *type      = aev->body.type;
  *size      = aev->body.size;
  *data      = LV2_ATOM_BODY(&aev->body);

  return true;
}

bool
lv2_evbuf_write(LV2_Evbuf_Iterator* iter,
                uint32_t            frames,
                uint32_t            subframes,
                uint32_t            type,
                uint32_t            size,
                const void*         data)
{
  (void)subframes;

  LV2_Atom_Sequence* aseq = &iter->evbuf->buf;
  if (iter->evbuf->capacity - sizeof(LV2_Atom) - aseq->atom.size <
      sizeof(LV2_Atom_Event) + size) {
    return false;
  }

  LV2_Atom_Event* aev =
    (LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) +
                      iter->offset);

  aev->time.frames = frames;
  aev->body.type   = type;
  aev->body.size   = size;
  memcpy(LV2_ATOM_BODY(&aev->body), data, size);

  size = lv2_atom_pad_size(sizeof(LV2_Atom_Event) + size);
  aseq->atom.size += size;
  iter->offset += size;

  return true;
}
