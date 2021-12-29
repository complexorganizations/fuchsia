// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/graphics/display/drivers/display/eld.h"

#include <src/lib/eld/eld.h>
#include <zxtest/zxtest.h>

namespace display {

TEST(EldTest, eld1) {
  static uint8_t edid1[] = {
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x1E, 0x6D, 0xB8, 0x5A, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x18, 0x01, 0x03, 0x80, 0x30, 0x1B, 0x78, 0xEA, 0x31, 0x35, 0xA5, 0x55, 0x4E,
      0xA1, 0x26, 0x0C, 0x50, 0x54, 0xA5, 0x4B, 0x00, 0x71, 0x4F, 0x81, 0x80, 0x95, 0x00, 0xB3,
      0x00, 0xA9, 0xC0, 0x81, 0x00, 0x81, 0xC0, 0x90, 0x40, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38,
      0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0xE0, 0x0E, 0x11, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
      0xFD, 0x00, 0x38, 0x4B, 0x1E, 0x53, 0x0F, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x00, 0x00, 0x00, 0xFC, 0x00, 0x4C, 0x47, 0x20, 0x49, 0x50, 0x53, 0x20, 0x46, 0x55, 0x4C,
      0x4C, 0x48, 0x44, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x64, 0x02, 0x03, 0x1D, 0xF1, 0x4A, 0x90, 0x04,
      0x03, 0x01, 0x14, 0x12, 0x05, 0x1F, 0x10, 0x13, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00,
      0x00, 0x65, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
      0x58, 0x2C, 0x45, 0x00, 0xE0, 0x0E, 0x11, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18, 0x71,
      0x1C, 0x16, 0x20, 0x58, 0x2C, 0x25, 0x00, 0xE0, 0x0E, 0x11, 0x00, 0x00, 0x9E, 0x01, 0x1D,
      0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xE0, 0x0E, 0x11, 0x00, 0x00,
      0x1E, 0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xE0, 0x0E,
      0x11, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xAE};

  edid::Edid edid;
  const char* err_msg = nullptr;
  ASSERT_TRUE(edid.Init(edid1, sizeof(edid1), &err_msg));
  fbl::Array<uint8_t> eld;
  ComputeEld(edid, eld);
  ASSERT_EQ(eld.size(), 36);
  EXPECT_EQ(eld[3], 0x10);  // Version 2.

  const char* monitor_name = "LG IPS FULLHD";
  size_t monitor_name_length = strlen(monitor_name);
  EXPECT_EQ(eld[sizeof(hda::EldHeader)], 0x60 | monitor_name_length);  // EDID version 3, mnl.
  EXPECT_TRUE(memcmp((char*)&eld[20], monitor_name, monitor_name_length) == 0);
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 1], 0x10);  // SAD count = 1, other fiels 0.

  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 12], 0x6d);  // Manufacturer id2 for LG.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 13], 0x1e);  // Manufacturer id1 for LG.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 14], 0xb8);  // Product Code2.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 15], 0x5a);  // Product Code1.

  // Short Audio Descriptor.
  size_t sad_start = sizeof(hda::EldHeader) + sizeof(hda::EldBaselinePart1) + monitor_name_length;
  EXPECT_EQ(eld[sad_start], 0x09);      // format = 1, num channels minus 1 = 1.
  EXPECT_EQ(eld[sad_start + 1], 0x07);  // sampling_frequencies 32k, 44.1k and 48k.
  EXPECT_EQ(eld[sad_start + 2], 0x07);  // All 4 bits for number of bits.
}

TEST(EldTest, VsyncWithEld2) {
  static uint8_t edid2[] = {
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x5A, 0x63, 0x34, 0x5B, 0x01, 0x01, 0x01,
      0x01, 0x2C, 0x1D, 0x01, 0x03, 0x80, 0x5E, 0x35, 0x78, 0x2E, 0x2E, 0xDD, 0xA6, 0x55, 0x4E,
      0x9A, 0x26, 0x0E, 0x47, 0x4A, 0xBF, 0xEF, 0x80, 0xD1, 0xC0, 0xB3, 0x00, 0xA9, 0x40, 0xA9,
      0xC0, 0x95, 0x00, 0x90, 0x40, 0x81, 0x80, 0x01, 0x01, 0x4D, 0xD0, 0x00, 0xA0, 0xF0, 0x70,
      0x3E, 0x80, 0x30, 0x20, 0x35, 0x00, 0xAD, 0x11, 0x32, 0x00, 0x00, 0x1A, 0x56, 0x5E, 0x00,
      0xA0, 0xA0, 0xA0, 0x29, 0x50, 0x2F, 0x20, 0x35, 0x00, 0xAD, 0x11, 0x32, 0x00, 0x00, 0x1A,
      0x00, 0x00, 0x00, 0xFD, 0x00, 0x32, 0x4B, 0x18, 0xA0, 0x3C, 0x01, 0x0A, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x56, 0x58, 0x34, 0x33, 0x38, 0x30, 0x20,
      0x53, 0x45, 0x52, 0x49, 0x45, 0x53, 0x01, 0x2E, 0x02, 0x03, 0x36, 0xF1, 0x54, 0x01, 0x03,
      0x04, 0x05, 0x07, 0x0A, 0x0B, 0x0E, 0x0F, 0x90, 0x12, 0x13, 0x14, 0x16, 0x1F, 0x60, 0x61,
      0x65, 0x66, 0x5D, 0x23, 0x09, 0x7F, 0x07, 0x83, 0x01, 0x00, 0x00, 0x67, 0x03, 0x0C, 0x00,
      0x10, 0x00, 0x38, 0x78, 0x67, 0xD8, 0x5D, 0xC4, 0x01, 0x78, 0x88, 0x03, 0xE4, 0x0F, 0x00,
      0x80, 0x07, 0x52, 0x6C, 0x80, 0xA0, 0x70, 0x70, 0x3E, 0x80, 0x30, 0x20, 0x3A, 0x00, 0xAD,
      0x11, 0x32, 0x00, 0x00, 0x1E, 0x1A, 0x68, 0x00, 0xA0, 0xF0, 0x38, 0x1F, 0x40, 0x30, 0x20,
      0xA3, 0x00, 0xAD, 0x11, 0x32, 0x00, 0x00, 0x18, 0xA3, 0x66, 0x00, 0xA0, 0xF0, 0x70, 0x1F,
      0x80, 0x30, 0x20, 0x35, 0x00, 0xAD, 0x11, 0x32, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x1A};

  edid::Edid edid;
  const char* err_msg = nullptr;
  ASSERT_TRUE(edid.Init(edid2, sizeof(edid2), &err_msg));
  fbl::Array<uint8_t> eld;
  ComputeEld(edid, eld);
  ASSERT_EQ(eld.size(), 36);
  EXPECT_EQ(eld[3], 0x10);  // Version 2.

  const char* monitor_name = "VX4380 SERIES";
  size_t monitor_name_length = strlen(monitor_name);
  EXPECT_EQ(eld[sizeof(hda::EldHeader)], 0x60 | monitor_name_length);  // EDID version 3, mnl.
  EXPECT_TRUE(memcmp((char*)&eld[20], monitor_name, monitor_name_length) == 0);
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 1], 0x10);  // SAD count = 1, other fiels 0.

  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 12], 0x63);  // Manufacturer id2 for ViewSonic.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 13], 0x5a);  // Manufacturer id1 for ViewSonic.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 14], 0x34);  // Product Code2.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 15], 0x5b);  // Product Code1.

  // Short Audio Descriptor.
  size_t sad_start = sizeof(hda::EldHeader) + sizeof(hda::EldBaselinePart1) + monitor_name_length;
  EXPECT_EQ(eld[sad_start], 0x09);      // format = 1, num channels minus 1 = 1.
  EXPECT_EQ(eld[sad_start + 1], 0x7F);  // All 7 bits for sampling_frequencies.
  EXPECT_EQ(eld[sad_start + 2], 0x07);  // All 4 bits for number of bits.
}

TEST(EldTest, VsyncWithEld3) {
  static uint8_t edid3[] = {
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x1E, 0x6D, 0x08, 0x5B, 0x15, 0x76, 0x01,
      0x00, 0x09, 0x1B, 0x01, 0x03, 0x80, 0x3C, 0x22, 0x78, 0xEA, 0x30, 0x35, 0xA7, 0x55, 0x4E,
      0xA3, 0x26, 0x0F, 0x50, 0x54, 0x21, 0x08, 0x00, 0x71, 0x40, 0x81, 0x80, 0x81, 0xC0, 0xA9,
      0xC0, 0xD1, 0xC0, 0x81, 0x00, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74, 0x00, 0x30, 0xF2, 0x70,
      0x5A, 0x80, 0xB0, 0x58, 0x8A, 0x00, 0x58, 0x54, 0x21, 0x00, 0x00, 0x1E, 0x56, 0x5E, 0x00,
      0xA0, 0xA0, 0xA0, 0x29, 0x50, 0x30, 0x20, 0x35, 0x00, 0x58, 0x54, 0x21, 0x00, 0x00, 0x1A,
      0x00, 0x00, 0x00, 0xFD, 0x00, 0x38, 0x3D, 0x1E, 0x87, 0x1E, 0x00, 0x0A, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x4C, 0x47, 0x20, 0x55, 0x6C, 0x74, 0x72,
      0x61, 0x20, 0x48, 0x44, 0x0A, 0x20, 0x01, 0xF7, 0x02, 0x03, 0x1D, 0x71, 0x46, 0x90, 0x22,
      0x05, 0x04, 0x03, 0x01, 0x23, 0x09, 0x07, 0x07, 0x6D, 0x03, 0x0C, 0x00, 0x10, 0x00, 0xB8,
      0x3C, 0x20, 0x00, 0x60, 0x01, 0x02, 0x03, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
      0x58, 0x2C, 0x45, 0x00, 0x58, 0x54, 0x21, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFF, 0x00,
      0x37, 0x30, 0x39, 0x4E, 0x54, 0x42, 0x4B, 0x32, 0x54, 0x37, 0x36, 0x35, 0x0A, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x8A};

  edid::Edid edid;
  const char* err_msg = nullptr;
  ASSERT_TRUE(edid.Init(edid3, sizeof(edid3), &err_msg));
  fbl::Array<uint8_t> eld;
  ComputeEld(edid, eld);
  ASSERT_EQ(eld.size(), 36);
  EXPECT_EQ(eld[3], 0x10);  // Version 2.

  const char* monitor_name = "LG Ultra HD";
  size_t monitor_name_length = strlen(monitor_name);
  EXPECT_EQ(eld[sizeof(hda::EldHeader)], 0x60 | monitor_name_length);  // EDID version 3, mnl.
  EXPECT_TRUE(memcmp((char*)&eld[20], monitor_name, monitor_name_length) == 0);
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 1], 0x10);  // SAD count = 1, other fiels 0.

  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 12], 0x6d);  // Manufacturer id2 for LG.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 13], 0x1e);  // Manufacturer id1 for LG.
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 14], 0x08);  // Product Code2
  EXPECT_EQ(eld[sizeof(hda::EldHeader) + 15], 0x5b);  // Product Code1

  // Short Audio Descriptor.
  size_t sad_start = sizeof(hda::EldHeader) + sizeof(hda::EldBaselinePart1) + monitor_name_length;
  EXPECT_EQ(eld[sad_start], 0x09);      // format = 1, num channels minus 1 = 1.
  EXPECT_EQ(eld[sad_start + 1], 0x07);  // sampling_frequencies 32k, 44.1k and 48k.
  EXPECT_EQ(eld[sad_start + 2], 0x07);  // All 4 bits for number of bits.

  EXPECT_EQ(eld[34], 0x00);  // Alignment bytes must be zero.
  EXPECT_EQ(eld[35], 0x00);  // Alignment bytes must be zero.
}

}  // namespace display
