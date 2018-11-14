// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "garnet/lib/overnet/vocabulary/slice.h"
#include "garnet/lib/overnet/vocabulary/status.h"

// Utilities to encode/decode slices via some codec.
//
// The general encoded format is:
// (codec type : u8) (encoded bytes)

namespace overnet {

// Collection of functions that define a single codec.
struct CodecVTable {
  const char* const name;
  Border (*border_for_source_size)(size_t size);
  StatusOr<Slice> (*encode)(Slice slice);
  StatusOr<Slice> (*decode)(Slice slice);
};

// Mapping from codec identifying byte to the codec implementation.
extern const CodecVTable* const kCodecVtable[256];

// Currently named codecs. Future implementations may expand this.
enum class Coding : uint8_t {
  Identity = 0,
};

static inline constexpr auto kDefaultCoding = Coding::Identity;

// Given a coding and a size, how much border should be allocated for a message?
inline Border BorderForSourceSize(Coding coding, size_t size) {
  return kCodecVtable[static_cast<uint8_t>(coding)]
      ->border_for_source_size(size)
      .WithAddedPrefix(1);
}

// Given a coding enum, get a name for the codec (or 'Unknown')
inline const char* CodingName(Coding coding) {
  return kCodecVtable[static_cast<uint8_t>(coding)]->name;
}

// Encode some data with a pre-selected coding.
inline StatusOr<Slice> Encode(Coding coding, Slice slice) {
  auto status =
      kCodecVtable[static_cast<uint8_t>(coding)]->encode(std::move(slice));
  if (status.is_error()) {
    return status;
  }
  return status->WithPrefix(
      1, [coding](uint8_t* p) { *p = static_cast<uint8_t>(coding); });
}

// Encode some data with an auto-selected coding.
inline StatusOr<Slice> Encode(Slice slice) {
  return Encode(kDefaultCoding, slice);
}

// Decode an encoded slice.
inline StatusOr<Slice> Decode(Slice slice) {
  if (slice.length() == 0) {
    return StatusOr<Slice>(StatusCode::INVALID_ARGUMENT,
                           "Can't decode an empty slice");
  }
  Coding coding = static_cast<Coding>(*slice.begin());
  slice.TrimBegin(1);
  return kCodecVtable[static_cast<uint8_t>(coding)]->decode(std::move(slice));
}

}  // namespace overnet
