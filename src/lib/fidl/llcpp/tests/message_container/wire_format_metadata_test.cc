// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/wire_format_metadata.h>

#include <gtest/gtest.h>

TEST(WireFormatMetadata, FromOpaque) {
  {
    ::fidl::WireFormatMetadata metadata = ::fidl::WireFormatMetadata::FromOpaque(
        // Magic number 1
        fidl_opaque_wire_format_metadata_t{0x100});

    EXPECT_EQ(fidl::internal::WireFormatVersion::kV1, metadata.wire_format_version());
    EXPECT_EQ(FIDL_WIRE_FORMAT_VERSION_V1, metadata.c_wire_format_version());
  }

  {
    ::fidl::WireFormatMetadata metadata = ::fidl::WireFormatMetadata::FromOpaque(
        // Magic number 1, and a V2 version flag
        fidl_opaque_wire_format_metadata_t{0x100 | 0x20000});

    EXPECT_EQ(fidl::internal::WireFormatVersion::kV2, metadata.wire_format_version());
    EXPECT_EQ(FIDL_WIRE_FORMAT_VERSION_V2, metadata.c_wire_format_version());
  }

  {
    ::fidl::WireFormatMetadata metadata = ::fidl::WireFormatMetadata::FromOpaque(
        // Invalid magic number
        fidl_opaque_wire_format_metadata_t{0x2});

    ASSERT_DEATH({ metadata.wire_format_version(); }, "Invalid");
    ASSERT_DEATH({ metadata.c_wire_format_version(); }, "Invalid");
  }
}

TEST(WireFormatMetadata, FromTransactionalHeader) {
  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::WireFormatMetadata::FromTransactionalHeader(fidl_message_header_t{
            .txid = 0,
            .at_rest_flags = {},
            .dynamic_flags = 0,
            .magic_number = kFidlWireFormatMagicNumberInitial,
            .ordinal = 0,
        });
    EXPECT_EQ(fidl::internal::WireFormatVersion::kV1, metadata.wire_format_version());
    EXPECT_EQ(FIDL_WIRE_FORMAT_VERSION_V1, metadata.c_wire_format_version());
  }

  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::WireFormatMetadata::FromTransactionalHeader(fidl_message_header_t{
            .txid = 0,
            .at_rest_flags = {FIDL_MESSAGE_HEADER_AT_REST_FLAGS_0_USE_VERSION_V2, 0},
            .dynamic_flags = 0,
            .magic_number = kFidlWireFormatMagicNumberInitial,
            .ordinal = 0,
        });
    EXPECT_EQ(fidl::internal::WireFormatVersion::kV2, metadata.wire_format_version());
    EXPECT_EQ(FIDL_WIRE_FORMAT_VERSION_V2, metadata.c_wire_format_version());
  }

  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::WireFormatMetadata::FromTransactionalHeader(fidl_message_header_t{
            .txid = 0,
            .at_rest_flags = {},
            .dynamic_flags = 0,
            // Invalid magic number
            .magic_number = 2,
            .ordinal = 0,
        });
    ASSERT_DEATH({ metadata.wire_format_version(); }, "Invalid");
    ASSERT_DEATH({ metadata.c_wire_format_version(); }, "Invalid");
  }
}

TEST(WireFormatMetadata, FromInternalVersion) {
  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::internal::WireFormatMetadataForVersion(::fidl::internal::WireFormatVersion::kV1);
    EXPECT_TRUE(metadata.is_valid());
    fidl_opaque_wire_format_metadata_t opaque = metadata.ToOpaque();
    EXPECT_EQ(0x100ull, opaque.metadata);
  }

  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::internal::WireFormatMetadataForVersion(::fidl::internal::WireFormatVersion::kV2);
    EXPECT_TRUE(metadata.is_valid());
    fidl_opaque_wire_format_metadata_t opaque = metadata.ToOpaque();
    EXPECT_EQ(0x100ull | 0x20000ull, opaque.metadata);
  }

  ASSERT_DEATH(
      {
        (void)::fidl::internal::WireFormatMetadataForVersion(
            static_cast<::fidl::internal::WireFormatVersion>(100));
      },
      "Unsupported");
}

TEST(WireFormatMetadata, ToOpaque) {
  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::WireFormatMetadata::FromOpaque(fidl_opaque_wire_format_metadata_t{});
    fidl_opaque_wire_format_metadata_t opaque = metadata.ToOpaque();
    EXPECT_EQ(0ull, opaque.metadata);
  }

  {
    ::fidl::WireFormatMetadata metadata =
        ::fidl::WireFormatMetadata::FromTransactionalHeader(fidl_message_header_t{
            .txid = 0,
            .at_rest_flags = {FIDL_MESSAGE_HEADER_AT_REST_FLAGS_0_USE_VERSION_V2, 0},
            .dynamic_flags = 0,
            .magic_number = kFidlWireFormatMagicNumberInitial,
            .ordinal = 0,
        });
    fidl_opaque_wire_format_metadata_t opaque = metadata.ToOpaque();
    EXPECT_EQ(0x100ull | 0x20000ull, opaque.metadata);
  }
}
