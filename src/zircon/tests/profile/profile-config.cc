// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/fdio.h>
#include <lib/fdio/io.h>
#include <lib/zx/channel.h>
#include <lib/zx/object.h>
#include <lib/zx/profile.h>
#include <lib/zx/thread.h>
#include <threads.h>
#include <zircon/errors.h>
#include <zircon/syscalls.h>

#include <string>
#include <unordered_set>

#include <zxtest/zxtest.h>

#include "zircon/system/ulib/profile/config.h"

namespace {

TEST(ProfileConfig, Parse) {
  auto result = zircon_profile::LoadConfigs("/pkg/data");
  ASSERT_TRUE(result.is_ok());

  {
    const auto iter = result->find("test.bringup.a:affinity");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Bringup);
    EXPECT_EQ(iter->second.info.flags,
              ZX_PROFILE_INFO_FLAG_CPU_MASK | ZX_PROFILE_INFO_FLAG_PRIORITY);
    EXPECT_EQ(iter->second.info.priority, 0);
    EXPECT_EQ(iter->second.info.cpu_affinity_mask.mask[0], 0b001);
  }

  {
    const auto iter = result->find("test.bringup.b:affinity");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Core);
    EXPECT_EQ(iter->second.info.flags,
              ZX_PROFILE_INFO_FLAG_CPU_MASK | ZX_PROFILE_INFO_FLAG_PRIORITY);
    EXPECT_EQ(iter->second.info.priority, 1);
    EXPECT_EQ(iter->second.info.cpu_affinity_mask.mask[0], 0b011);
  }

  {
    const auto iter = result->find("test.core.a");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Core);
    EXPECT_EQ(iter->second.info.flags, ZX_PROFILE_INFO_FLAG_DEADLINE);
    EXPECT_EQ(iter->second.info.deadline_params.capacity, 5'000'000);
    EXPECT_EQ(iter->second.info.deadline_params.relative_deadline, 10'000'000);
    EXPECT_EQ(iter->second.info.deadline_params.period, 10'000'000);
  }

  {
    const auto iter = result->find("test.bringup.a");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Core);
    EXPECT_EQ(iter->second.info.flags, ZX_PROFILE_INFO_FLAG_PRIORITY);
    EXPECT_EQ(iter->second.info.priority, 10);
  }

  {
    const auto iter = result->find("test.product.a");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Product);
    EXPECT_EQ(iter->second.info.flags, ZX_PROFILE_INFO_FLAG_PRIORITY);
    EXPECT_EQ(iter->second.info.priority, 25);
  }

  {
    const auto iter = result->find("test.core.a:affinity");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Product);
    EXPECT_EQ(iter->second.info.flags,
              ZX_PROFILE_INFO_FLAG_CPU_MASK | ZX_PROFILE_INFO_FLAG_DEADLINE);
    EXPECT_EQ(iter->second.info.deadline_params.capacity, 6'000'000);
    EXPECT_EQ(iter->second.info.deadline_params.relative_deadline, 15'000'000);
    EXPECT_EQ(iter->second.info.deadline_params.period, 20'000'000);
    EXPECT_EQ(iter->second.info.cpu_affinity_mask.mask[0], 0b110);
  }

  {
    const auto iter = result->find("test.bringup.b");
    ASSERT_TRUE(iter != result->end());
    EXPECT_EQ(iter->second.scope, zircon_profile::ProfileScope::Product);
    EXPECT_EQ(iter->second.info.flags, ZX_PROFILE_INFO_FLAG_PRIORITY);
    EXPECT_EQ(iter->second.info.priority, 20);
  }

  const std::unordered_set<std::string> expected_profiles{
      "test.product.a", "test.core.a:affinity",    "test.bringup.a:affinity",
      "test.bringup.b", "test.bringup.b:affinity", "test.core.a",
      "test.bringup.a",
  };

  for (const auto& [key, value] : *result) {
    EXPECT_NE(expected_profiles.end(), expected_profiles.find(key));
  }
}

}  // anonymous namespace
