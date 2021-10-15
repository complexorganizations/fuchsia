// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_CHANNEL_H_
#define LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_CHANNEL_H_

#include <lib/fidl/llcpp/internal/transport.h>
#include <lib/zx/channel.h>
#include <zircon/syscalls.h>

namespace fidl::internal {

struct ChannelTransport {
  using OwnedType = zx::channel;
  using UnownedType = zx::unowned_channel;
  static const TransportVTable VTable;
  static const EncodingConfiguration EncodingConfiguration;
};

AnyTransport MakeAnyTransport(zx::channel channel);
AnyUnownedTransport MakeAnyUnownedTransport(const zx::channel& channel);
AnyUnownedTransport MakeAnyUnownedTransport(const zx::unowned_channel& channel);

}  // namespace fidl::internal

#endif  // LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_CHANNEL_H_
