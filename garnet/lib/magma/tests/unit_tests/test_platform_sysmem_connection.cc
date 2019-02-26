// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_sysmem_connection.h"
#include "gtest/gtest.h"

class TestPlatformSysmemConnection {
public:
    static void TestCreateBuffer()
    {
        auto connection = magma::PlatformSysmemConnection::Create();

        ASSERT_NE(nullptr, connection.get());

        std::unique_ptr<magma::PlatformBuffer> buffer;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->AllocateBuffer(0, 16384, &buffer));
        ASSERT_TRUE(buffer != nullptr);
        EXPECT_LE(16384u, buffer->size());
    }

    static void TestCreate()
    {
        auto connection = magma::PlatformSysmemConnection::Create();

        ASSERT_NE(nullptr, connection.get());

        std::unique_ptr<magma::PlatformBuffer> buffer;
        std::unique_ptr<magma::PlatformBufferDescription> description;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->AllocateTexture(0, MAGMA_FORMAT_R8G8B8A8, 128, 64,
                                                               &buffer, &description));
        EXPECT_TRUE(buffer != nullptr);
        ASSERT_TRUE(description != nullptr);
        EXPECT_TRUE(description->planes[0].bytes_per_row >= 128 * 4);
    }

    static void TestSetConstraints()
    {
        auto connection = magma::PlatformSysmemConnection::Create();

        ASSERT_NE(nullptr, connection.get());

        uint32_t token;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->CreateBufferCollectionToken(&token).get());
        std::unique_ptr<magma::PlatformBufferCollection> collection;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->ImportBufferCollection(token, &collection).get());

        magma_buffer_format_constraints_t buffer_constraints;

        buffer_constraints.count = 1;
        buffer_constraints.usage = 0;
        buffer_constraints.secure_permitted = false;
        buffer_constraints.secure_required = false;
        std::unique_ptr<magma::PlatformBufferConstraints> constraints;
        EXPECT_EQ(MAGMA_STATUS_OK,
                  connection->CreateBufferConstraints(&buffer_constraints, &constraints).get());

        // Create a set of basic 512x512 RGBA image constraints.
        magma_image_format_constraints_t image_constraints;
        image_constraints.image_format = MAGMA_FORMAT_R8G8B8A8;
        image_constraints.has_format_modifier = false;
        image_constraints.format_modifier = 0;
        image_constraints.width = 512;
        image_constraints.height = 512;
        image_constraints.layers = 1;
        image_constraints.bytes_per_row_divisor = 1;
        image_constraints.min_bytes_per_row = 0;

        EXPECT_NE(MAGMA_STATUS_OK,
                  constraints->SetImageFormatConstraints(1, &image_constraints).get());
        EXPECT_EQ(MAGMA_STATUS_OK,
                  constraints->SetImageFormatConstraints(0, &image_constraints).get());
        EXPECT_EQ(MAGMA_STATUS_OK,
                  constraints->SetImageFormatConstraints(1, &image_constraints).get());
        EXPECT_EQ(MAGMA_STATUS_OK, collection->SetConstraints(constraints.get()).get());
    }

    static void TestIntelTiling()
    {
        auto connection = magma::PlatformSysmemConnection::Create();

        ASSERT_NE(nullptr, connection.get());

        uint32_t token;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->CreateBufferCollectionToken(&token).get());
        std::unique_ptr<magma::PlatformBufferCollection> collection;
        EXPECT_EQ(MAGMA_STATUS_OK, connection->ImportBufferCollection(token, &collection).get());

        magma_buffer_format_constraints_t buffer_constraints;

        buffer_constraints.count = 1;
        buffer_constraints.usage = 0;
        buffer_constraints.secure_permitted = false;
        buffer_constraints.secure_required = false;
        std::unique_ptr<magma::PlatformBufferConstraints> constraints;
        EXPECT_EQ(MAGMA_STATUS_OK,
                  connection->CreateBufferConstraints(&buffer_constraints, &constraints).get());

        // Create Intel X-tiling
        magma_image_format_constraints_t image_constraints;
        image_constraints.image_format = MAGMA_FORMAT_R8G8B8A8;
        image_constraints.has_format_modifier = true;
        image_constraints.format_modifier = MAGMA_FORMAT_MODIFIER_INTEL_I915_X_TILED;
        image_constraints.width = 512;
        image_constraints.height = 512;
        image_constraints.layers = 1;
        image_constraints.bytes_per_row_divisor = 1;
        image_constraints.min_bytes_per_row = 0;

        EXPECT_EQ(MAGMA_STATUS_OK,
                  constraints->SetImageFormatConstraints(0, &image_constraints).get());
        EXPECT_EQ(MAGMA_STATUS_OK, collection->SetConstraints(constraints.get()).get());
        std::unique_ptr<magma::PlatformBufferDescription> description;
        EXPECT_EQ(MAGMA_STATUS_OK, collection->GetBufferDescription(0, &description).get());
        EXPECT_TRUE(description->has_format_modifier);
        EXPECT_EQ(MAGMA_FORMAT_MODIFIER_INTEL_I915_X_TILED, description->format_modifier);
    }
};

TEST(PlatformSysmemConnection, CreateBuffer) { TestPlatformSysmemConnection::TestCreateBuffer(); }

TEST(PlatformSysmemConnection, Create) { TestPlatformSysmemConnection::TestCreate(); }

TEST(PlatformSysmemConnection, SetConstraints)
{
    TestPlatformSysmemConnection::TestSetConstraints();
}

TEST(PlatformSysmemConnection, IntelTiling) { TestPlatformSysmemConnection::TestIntelTiling(); }
