// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "styling.h"

#include <assert.h>
#include <memory.h>

#include "core_c.h"
#include "spinel/spinel.h"
#include "spinel/spinel_opcodes.h"

//
//
//

static uint32_t
spinel_styling_cmd_base_count(uint32_t const base, uint32_t const n)
{
  assert(base < SPN_STYLING_CMDS_MAX_BASE);
  assert(n <= SPN_STYLING_CMDS_MAX_COUNT);

  return base | (n << SPN_STYLING_CMDS_OFFSET_COUNT);
}

//
//
//

spinel_result_t
spinel_styling_retain(spinel_styling_t styling)
{
  assert(styling->ref_count >= 1);

  ++styling->ref_count;

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_release(spinel_styling_t styling)
{
  assert(styling->ref_count >= 1);

  if (--styling->ref_count == 0)
    {
      return styling->release(styling->impl);
    }
  else
    {
      return SPN_SUCCESS;
    }
}

spinel_result_t
spinel_styling_seal(spinel_styling_t styling)
{
  return styling->seal(styling->impl);
}

spinel_result_t
spinel_styling_unseal(spinel_styling_t styling)
{
  return styling->unseal(styling->impl);
}

spinel_result_t
spinel_styling_reset(spinel_styling_t styling)
{
  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  styling->dwords.next = styling->layers.count * SPN_STYLING_LAYER_COUNT_DWORDS;

  return SPN_SUCCESS;
}

//
// FIXME -- various robustifications can be made to this builder but
// we don't want to make this heavyweight too soon
//
// - out of range layer_id is an error
// - extras[] overflow is an error
//

spinel_result_t
spinel_styling_group_alloc(spinel_styling_t styling, spinel_group_id * const group_id)
{
  assert(styling->dwords.next + SPN_STYLING_GROUP_COUNT_DWORDS <= styling->dwords.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  *group_id = styling->dwords.next;

  styling->dwords.next += SPN_STYLING_GROUP_COUNT_DWORDS;

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_enter(spinel_styling_t      styling,
                           spinel_group_id const group_id,
                           uint32_t const        n,
                           uint32_t ** const     cmds)
{
  assert(styling->dwords.next + n <= styling->dwords.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  if (n == 0)
    {
      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_CMDS_ENTER] = 0;

      if (cmds != NULL)
        {
          *cmds = NULL;
        }
    }
  else
    {
      assert(cmds != NULL);

      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_CMDS_ENTER] =
        spinel_styling_cmd_base_count(styling->dwords.next, n);

      *cmds = styling->extent + styling->dwords.next;

      styling->dwords.next += n;
    }

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_leave(spinel_styling_t      styling,
                           spinel_group_id const group_id,
                           uint32_t const        n,
                           uint32_t ** const     cmds)
{
  assert(styling->dwords.next + n <= styling->dwords.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  if (n == 0)
    {
      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_CMDS_LEAVE] = 0;

      if (cmds != NULL)
        {
          *cmds = NULL;
        }
    }
  else
    {
      assert(cmds != NULL);

      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_CMDS_LEAVE] =
        spinel_styling_cmd_base_count(styling->dwords.next, n);

      *cmds = styling->extent + styling->dwords.next;

      styling->dwords.next += n;
    }

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_parents(spinel_styling_t      styling,
                             spinel_group_id const group_id,
                             uint32_t const        n,
                             uint32_t ** const     parents)
{
  assert(styling->dwords.next + n <= styling->dwords.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  if (n == 0)
    {
      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_PARENTS_DEPTH] = 0;
      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_PARENTS_BASE]  = UINT32_MAX;

      if (parents != NULL)
        {
          *parents = NULL;
        }
    }
  else
    {
      assert(parents != NULL);

      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_PARENTS_DEPTH] = n;
      styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_PARENTS_BASE]  = styling->dwords.next;

      *parents = styling->extent + styling->dwords.next;
    }

  styling->dwords.next += n;

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_range_lo(spinel_styling_t      styling,
                              spinel_group_id const group_id,
                              spinel_layer_id const layer_lo)
{
  assert(layer_lo < styling->layers.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_RANGE_LO] = layer_lo;

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_range_hi(spinel_styling_t      styling,
                              spinel_group_id const group_id,
                              spinel_layer_id const layer_hi)
{
  assert(layer_hi < styling->layers.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  styling->extent[group_id + SPN_STYLING_GROUP_OFFSET_RANGE_HI] = layer_hi;

  return SPN_SUCCESS;
}

spinel_result_t
spinel_styling_group_layer(spinel_styling_t              styling,
                           spinel_group_id const         group_id,
                           spinel_layer_id const         layer_id,
                           uint32_t const                n,
                           spinel_styling_cmd_t ** const cmds)
{
  assert(layer_id < styling->layers.count);
  assert(styling->dwords.next + n <= styling->dwords.count);

  spinel_result_t const res = styling->unseal(styling->impl);

  if (res)
    {
      return res;
    }

  styling->extent[layer_id * SPN_STYLING_LAYER_COUNT_DWORDS + SPN_STYLING_LAYER_OFFSET_CMDS] =
    spinel_styling_cmd_base_count(styling->dwords.next, n);

  styling->extent[layer_id * SPN_STYLING_LAYER_COUNT_DWORDS + SPN_STYLING_LAYER_OFFSET_PARENT] =
    group_id;

  *cmds = styling->extent + styling->dwords.next;

  styling->dwords.next += n;

  return SPN_SUCCESS;
}

//
// FIXME(allanmac) -- get rid of these x86'isms ASAP -- let compiler figure it
// out with a vector type
//

static void
spinel_convert_colors_4(float const * const fp32v4, uint32_t * const u32v2)
{
#if 0

  //
  // FIXME(allanmac): use x86 and ARM accelerated conversion instrinsics
  //
  __m128i u128 = _mm_cvtps_ph(*(__m128 const *)fp32v4, 0);

  memcpy(u32v2,&u128,sizeof(uint32_t) * 2);

#else

  //
  // Default leverages Clang's always-supported fp16 storage format
  //
  union
  {
    uint32_t * u32;
    __fp16 *   fp16;
  } pun = { .u32 = u32v2 };

  pun.fp16[0] = (__fp16)fp32v4[0];
  pun.fp16[1] = (__fp16)fp32v4[1];
  pun.fp16[2] = (__fp16)fp32v4[2];
  pun.fp16[3] = (__fp16)fp32v4[3];

#endif
}

#if 0  // NOT USED YET -- need to define missing caller

static void
spinel_convert_colors_8(float const * const fp32v8, uint32_t * const u32v4)
{
#if 0

  //
  // FIXME(allanmac): use x86 and ARM accelerated conversion instrinsics
  //
  __m128i u128 = _mm256_cvtps_ph(*(__m256 *)fp32v8, 0);

  memcpy(u32v4,&u128,sizeof(uint32_t) * 4);

#else

  //
  // Default leverages Clang's always-supported fp16 storage format
  //
  union
  {
    uint32_t * u32;
    __fp16 *   fp16;
  } pun = { .u32 = u32v4 };

  pun.fp16[0] = (__fp16)fp32v8[0];
  pun.fp16[1] = (__fp16)fp32v8[1];
  pun.fp16[2] = (__fp16)fp32v8[2];
  pun.fp16[3] = (__fp16)fp32v8[3];
  pun.fp16[4] = (__fp16)fp32v8[4];
  pun.fp16[5] = (__fp16)fp32v8[5];
  pun.fp16[6] = (__fp16)fp32v8[6];
  pun.fp16[7] = (__fp16)fp32v8[7];

#endif
}

#endif

//
//
//

static void
spinel_styling_layer_cmd_rgba_encoder(spinel_styling_cmd_t * const cmds,
                                      spinel_styling_cmd_t const   opcode,
                                      float const                  rgba[4])
{
  uint32_t u32v2[2];

  spinel_convert_colors_4(rgba, u32v2);

  cmds[0] = opcode;
  cmds[1] = u32v2[0];
  cmds[2] = u32v2[1];
}

void
spinel_styling_background_over_encoder(spinel_styling_cmd_t * cmds, float const rgba[4])
{
  spinel_styling_layer_cmd_rgba_encoder(cmds, SPN_STYLING_OPCODE_COLOR_ACC_OVER_BACKGROUND, rgba);
}

void
spinel_styling_layer_fill_rgba_encoder(spinel_styling_cmd_t * cmds, float const rgba[4])
{
  // encode a solid fill
  spinel_styling_layer_cmd_rgba_encoder(cmds, SPN_STYLING_OPCODE_COLOR_FILL_SOLID, rgba);
}

//
//
//
