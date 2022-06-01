// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSTEM_ULIB_PROFILE_H_
#define ZIRCON_SYSTEM_ULIB_PROFILE_H_

#include <lib/fitx/result.h>
#include <zircon/syscalls.h>

#include <string>
#include <unordered_map>

#include <fbl/enum_bits.h>

namespace zircon_profile {

enum class ProfileScope {
  None = 0,
  Bringup,
  Core,
  Product,
};
FBL_ENABLE_ENUM_BITS(ProfileScope)

struct Profile {
  ProfileScope scope;
  zx_profile_info_t info;
};

using ProfileMap = std::unordered_map<std::string, Profile>;

fitx::result<std::string, ProfileMap> LoadConfigs(const std::string& config_path);

}  // namespace zircon_profile

#endif  // ZIRCON_SYSTEM_ULIB_PROFILE_H_
