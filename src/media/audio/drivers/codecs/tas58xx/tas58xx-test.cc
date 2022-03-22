// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tas58xx.h"

#include <lib/ddk/metadata.h>
#include <lib/ddk/platform-defs.h>
#include <lib/mock-i2c/mock-i2c.h>
#include <lib/simple-codec/simple-codec-client.h>
#include <lib/simple-codec/simple-codec-helper.h>
#include <lib/sync/completion.h>

#include <string>

#include <zxtest/zxtest.h>

#include "src/devices/testing/mock-ddk/mock-device.h"

namespace audio {

namespace audio_fidl = ::fuchsia::hardware::audio;
namespace signal_fidl = ::fuchsia::hardware::audio::signalprocessing;

struct Tas58xxCodec : public Tas58xx {
  explicit Tas58xxCodec(zx_device_t* parent, const ddk::I2cChannel& i2c) : Tas58xx(parent, i2c) {}
  codec_protocol_t GetProto() { return {&this->codec_protocol_ops_, this}; }
  uint64_t GetTopologyId() { return Tas58xx::GetTopologyId(); }
  uint64_t GetAglPeId() { return Tas58xx::GetAglPeId(); }
  uint64_t GetEqPeId() { return Tas58xx::GetEqPeId(); }
  zx_status_t SetBand(bool enabled, size_t index, uint32_t frequency, float Q, float gain_db) {
    return Tas58xx::SetBand(enabled, index, frequency, Q, gain_db);
  }
};

TEST(Tas58xxTest, GoodSetDai) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00}, ZX_ERR_INTERNAL);  // Error will retry.
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00}, ZX_ERR_INTERNAL);  // Error will retry.
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00}, ZX_OK);  // Check DIE ID, no error now.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 3;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    mock_i2c.ExpectWriteStop({0x33, 0x03});  // 32 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x00});  // Keep data start sclk.
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    auto codec_format_info = client.SetDaiFormat(std::move(format));
    // 5ms turn on delay expected.
    ASSERT_OK(codec_format_info.status_value());
    EXPECT_EQ(zx::msec(5).get(), codec_format_info->turn_on_delay());
    EXPECT_FALSE(codec_format_info->has_turn_off_delay());
  }

  // One channel is ok.
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 2;  // only one channel is ok.
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    mock_i2c.ExpectWriteStop({0x33, 0x00});  // 16 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x00});  // Keep data start sclk.
    format.bits_per_sample = 16;
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    ASSERT_OK(client.SetDaiFormat(std::move(format)));
  }

  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 3;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    mock_i2c.ExpectWriteStop({0x33, 0x00});  // 16 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x00});  // Keep data start sclk.
    format.bits_per_sample = 16;
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    ASSERT_OK(client.SetDaiFormat(std::move(format)));
  }

  {
    audio::DaiFormat format = {};
    format.number_of_channels = 4;
    format.channels_to_use_bitmask = 0xc;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::TDM1;
    format.frame_rate = 48000;
    format.bits_per_slot = 16;
    format.bits_per_sample = 16;
    mock_i2c.ExpectWriteStop({0x33, 0x14});  // TDM/DSP, I2S_LRCLK_PULSE < 8 SCLK, 16 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x20});  // Data start sclk at 32 bits.
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    ASSERT_OK(client.SetDaiFormat(std::move(format)));
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, BadSetDai) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  // Blank format.
  {
    audio::DaiFormat format = {};
    auto formats = client.GetDaiFormats();
    EXPECT_FALSE(IsDaiFormatSupported(format, formats.value()));
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_INVALID_ARGS, format_info.status_value());
  }

  // Almost good format (wrong frame_format).
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 3;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::STEREO_LEFT;  // This must fail.
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    auto formats = client.GetDaiFormats();
    EXPECT_FALSE(IsDaiFormatSupported(format, formats.value()));
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, format_info.status_value());
  }

  // Almost good format (wrong channels).
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 1;
    format.channels_to_use_bitmask = 1;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    auto formats = client.GetDaiFormats();
    EXPECT_FALSE(IsDaiFormatSupported(format, formats.value()));
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, format_info.status_value());
  }

  // Almost good format (wrong mask).
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 4;  // TAS58xx requires use only the first 2 bits.
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    auto formats = client.GetDaiFormats();
    EXPECT_TRUE(IsDaiFormatSupported(format, formats.value()));  // bitmask not checked here.
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, format_info.status_value());
  }

  // Almost good format (wrong rate).
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 3;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 1234;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    auto formats = client.GetDaiFormats();
    EXPECT_FALSE(IsDaiFormatSupported(format, formats.value()));
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, format_info.status_value());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, GetDai) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  auto formats = client.GetDaiFormats();
  EXPECT_EQ(formats.value().number_of_channels.size(), 2);
  EXPECT_EQ(formats.value().number_of_channels[0], 2);
  EXPECT_EQ(formats.value().number_of_channels[1], 4);
  EXPECT_EQ(formats.value().sample_formats.size(), 1);
  EXPECT_EQ(formats.value().sample_formats[0], SampleFormat::PCM_SIGNED);
  EXPECT_EQ(formats.value().frame_formats.size(), 2);
  EXPECT_EQ(formats.value().frame_formats[0], FrameFormat::I2S);
  EXPECT_EQ(formats.value().frame_formats[1], FrameFormat::TDM1);
  EXPECT_EQ(formats.value().frame_rates.size(), 2);
  EXPECT_EQ(formats.value().frame_rates[0], 48'000);
  EXPECT_EQ(formats.value().frame_rates[1], 96'000);
  EXPECT_EQ(formats.value().bits_per_slot.size(), 2);
  EXPECT_EQ(formats.value().bits_per_slot[0], 16);
  EXPECT_EQ(formats.value().bits_per_slot[1], 32);
  EXPECT_EQ(formats.value().bits_per_sample.size(), 2);
  EXPECT_EQ(formats.value().bits_per_sample[0], 16);
  EXPECT_EQ(formats.value().bits_per_sample[1], 32);

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, GetInfo5805) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.
    auto info = client.GetInfo();
    EXPECT_EQ(info.value().unique_id.compare(""), 0);
    EXPECT_EQ(info.value().manufacturer.compare("Texas Instruments"), 0);
    EXPECT_EQ(info.value().product_name.compare("TAS5805m"), 0);
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, GetInfo5825) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.
    auto info = client.GetInfo();
    EXPECT_EQ(info.value().unique_id.compare(""), 0);
    EXPECT_EQ(info.value().manufacturer.compare("Texas Instruments"), 0);
    EXPECT_EQ(info.value().product_name.compare("TAS5825m"), 0);
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, CheckState) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    auto info = client.IsBridgeable();
    EXPECT_EQ(info.value(), false);

    auto format = client.GetGainFormat();
    EXPECT_EQ(format.value().min_gain, -103.0);
    EXPECT_EQ(format.value().max_gain, 24.0);
    EXPECT_EQ(format.value().gain_step, 0.5);
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetGain) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    mock_i2c
        .ExpectWriteStop({0x4c, 0x48})  // digital vol -12dB.
        .ExpectWrite({0x03})
        .ExpectReadStop({0x00})
        .ExpectWriteStop({0x03, 0x00});  // Muted = false.
    GainState gain({.gain = -12.f, .muted = false, .agc_enabled = false});
    client.SetGainState(gain);
  }

  {
    mock_i2c
        .ExpectWriteStop({0x4c, 0x60})  // digital vol -24dB.
        .ExpectWrite({0x03})
        .ExpectReadStop({0x00})
        .ExpectWriteStop({0x03, 0x08});  // Muted = true.
    GainState gain({.gain = -24.f, .muted = true, .agc_enabled = false});
    client.SetGainState(gain);
  }

  // Make a 2-way call to make sure the server (we know single threaded) completed previous calls.
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.
  auto unused = client.GetInfo();
  static_cast<void>(unused);

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetAglSignalProcessing) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  // AGL enabled.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0xc0, 0x00, 0x00, 0x00})  // Enable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.
    client.SetAgl(true);
  }

  // Make a 2-way call to make sure the server (we know single threaded) completed previous calls.
  {
    mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.
    auto unused = client.GetInfo();
    static_cast<void>(unused);
  }

  // AGL disabled.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0x40, 0x00, 0x00, 0x00})  // Disable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.
    client.SetAgl(false);
  }

  // Make a 2-way call to make sure the server (we know single threaded) completed previous calls.
  {
    mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x00});  // Check DIE ID.
    auto unused = client.GetInfo();
    static_cast<void>(unused);
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, GetTopologySignalProcessing) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get one topology with an AGL processing element.
  signal_fidl::Reader_GetTopologies_Result result;
  ASSERT_OK(signal_processing_client->GetTopologies(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().topologies.size(), 1);
  ASSERT_EQ(result.response().topologies[0].id(), codec->GetTopologyId());
  ASSERT_EQ(result.response().topologies[0].processing_elements_edge_pairs().size(), 1);
  ASSERT_EQ(result.response()
                .topologies[0]
                .processing_elements_edge_pairs()[0]
                .processing_element_id_from,
            codec->GetAglPeId());
  ASSERT_EQ(
      result.response().topologies[0].processing_elements_edge_pairs()[0].processing_element_id_to,
      codec->GetAglPeId());

  // Set the only topology must work.
  signal_fidl::SignalProcessing_SetTopology_Result result2;
  ASSERT_OK(signal_processing_client->SetTopology(codec->GetTopologyId(), &result2));
  ASSERT_FALSE(result2.is_err());

  // Set the an incorrect topology id must fail.
  signal_fidl::SignalProcessing_SetTopology_Result result3;
  ASSERT_OK(signal_processing_client->SetTopology(codec->GetTopologyId() + 1, &result3));
  ASSERT_TRUE(result3.is_err());

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SignalProcessingConnectTooManyConnections) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  // First kNumberOfConnectionsSucceed connections succeed in making a 2-way call.
  constexpr size_t kNumberOfConnectionsSucceed = 8;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing>
      signal_processing_request[kNumberOfConnectionsSucceed];
  fidl::SynchronousInterfacePtr<signal_fidl::SignalProcessing>
      signal_processing_client[kNumberOfConnectionsSucceed];
  for (size_t i = 0; i < kNumberOfConnectionsSucceed; ++i) {
    fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
    signal_processing_request[i] = signal_processing_handle.NewRequest();
    ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request[i])));
    signal_processing_client[i] = signal_processing_handle.BindSync();
    signal_fidl::Reader_GetTopologies_Result result;
    ASSERT_OK(signal_processing_client[i]->GetTopologies(&result));
    ASSERT_FALSE(result.is_err());
  }

  // Connection number kNumberOfConnectionsSucceed + 1 fails to make a 2-way call.
  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle2;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request2 =
      signal_processing_handle2.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request2)));
  fidl::SynchronousInterfacePtr signal_processing_client2 = signal_processing_handle2.BindSync();
  signal_fidl::Reader_GetTopologies_Result result2;
  ASSERT_EQ(signal_processing_client2->GetTopologies(&result2), ZX_ERR_PEER_CLOSED);

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, WatchAgl) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs one AGL and one EQUALIZER.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[0].type(),
            signal_fidl::ElementType::AUTOMATIC_GAIN_LIMITER);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // AGL enabled.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0xc0, 0x00, 0x00, 0x00})  // Enable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.

    // Control with enabled = true.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState state;
    state.set_enabled(true);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[0].id(), std::move(state), &result_enable));
    ASSERT_FALSE(result_enable.is_err());

    signal_fidl::ElementState state_received;
    signal_processing_client->WatchElementState(result.response().processing_elements[0].id(),
                                                &state_received);
    ASSERT_TRUE(state_received.has_enabled());
    ASSERT_TRUE(state_received.enabled());
  }

  // AGL disabled.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0x40, 0x00, 0x00, 0x00})  // Disable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.

    // Control with enabled = false.
    signal_fidl::SignalProcessing_SetElementState_Result state_result;
    signal_fidl::ElementState state;
    state.set_enabled(false);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[0].id(), std::move(state), &state_result));
    ASSERT_FALSE(state_result.is_err());

    signal_fidl::ElementState state_received;
    signal_processing_client->WatchElementState(result.response().processing_elements[0].id(),
                                                &state_received);
    ASSERT_TRUE(state_received.has_enabled());
    ASSERT_FALSE(state_received.enabled());
  }
}

TEST(Tas58xxTest, WatchAglUpdates) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs one AGL and one EQUALIZER.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[0].type(),
            signal_fidl::ElementType::AUTOMATIC_GAIN_LIMITER);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // A Watch after a SetPE disable must reply since the PE state changed.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0xc0, 0x00, 0x00, 0x00})  // Enable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.

    // Control with enabled = true.
    signal_fidl::SignalProcessing_SetElementState_Result state_result;
    signal_fidl::ElementState state;
    state.set_enabled(true);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[0].id(), std::move(state), &state_result));
    ASSERT_FALSE(state_result.is_err());

    signal_fidl::ElementState state_received;
    signal_processing_client->WatchElementState(result.response().processing_elements[0].id(),
                                                &state_received);
    ASSERT_TRUE(state_received.has_enabled());
    ASSERT_TRUE(state_received.enabled());
  }

  // A Watch potentially before a SetPE disable must reply since the PE state changed.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0x40, 0x00, 0x00, 0x00})  // Disable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.

    std::thread th([&]() {
      signal_fidl::ElementState state_received;
      signal_processing_client->WatchElementState(result.response().processing_elements[0].id(),
                                                  &state_received);
      ASSERT_TRUE(state_received.has_enabled());
      ASSERT_FALSE(state_received.enabled());
    });

    // Not required for the test to pass, but rather makes it likely for the watch to start before
    // the SetPE, either way the test is valid.
    zx::nanosleep(zx::deadline_after(zx::msec(10)));

    // Control with enabled = false.
    signal_fidl::SignalProcessing_SetElementState_Result state_result;
    signal_fidl::ElementState state;
    state.set_enabled(false);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[0].id(), std::move(state), &state_result));
    ASSERT_FALSE(state_result.is_err());

    th.join();
  }

  // A Watch after a previous watch with a reply triggered by SetPE must reply if we change the
  // PE state with a new SetPE.
  {
    mock_i2c
        .ExpectWriteStop({0x7f, 0x8c})                    // book 0x8c.
        .ExpectWriteStop({0x00, 0x2c})                    // page 0x2c.
        .ExpectWriteStop({0x68, 0xc0, 0x00, 0x00, 0x00})  // Enable AGL.
        .ExpectWriteStop({0x00, 0x00})                    // page 0.
        .ExpectWriteStop({0x7f, 0x00});                   // book 0.

    // Control with enabled = true.
    signal_fidl::SignalProcessing_SetElementState_Result state_result;
    signal_fidl::ElementState state;
    state.set_enabled(true);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[0].id(), std::move(state), &state_result));
    ASSERT_FALSE(state_result.is_err());

    signal_fidl::ElementState state_received;
    signal_processing_client->WatchElementState(result.response().processing_elements[0].id(),
                                                &state_received);
    ASSERT_TRUE(state_received.has_enabled());
    ASSERT_TRUE(state_received.enabled());
  }
}

TEST(Tas58xxTest, WatchEqualizer) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs one AGL and one EQUALIZER.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[0].type(),
            signal_fidl::ElementType::AUTOMATIC_GAIN_LIMITER);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  signal_fidl::ElementState state_received;
  signal_processing_client->WatchElementState(result.response().processing_elements[1].id(),
                                              &state_received);
  ASSERT_TRUE(state_received.has_enabled());
  ASSERT_TRUE(state_received.enabled());
  ASSERT_TRUE(state_received.has_type_specific());
  ASSERT_TRUE(state_received.type_specific().is_equalizer());
  ASSERT_TRUE(state_received.type_specific().equalizer().has_bands_state());

  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[0].has_id());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[1].has_id());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[2].has_id());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[3].has_id());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[4].has_id());
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[0].id(), 0);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[1].id(), 1);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[2].id(), 2);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[3].id(), 3);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[4].id(), 4);

  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[0].has_type());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[1].has_type());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[2].has_type());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[3].has_type());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[4].has_type());
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[0].type(),
            signal_fidl::EqualizerBandType::PEAK);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[1].type(),
            signal_fidl::EqualizerBandType::PEAK);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[2].type(),
            signal_fidl::EqualizerBandType::PEAK);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[3].type(),
            signal_fidl::EqualizerBandType::PEAK);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[4].type(),
            signal_fidl::EqualizerBandType::PEAK);

  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[0].has_q());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[1].has_q());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[2].has_q());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[3].has_q());
  ASSERT_TRUE(state_received.type_specific().equalizer().bands_state()[4].has_q());
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[0].q(), 1.f);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[1].q(), 1.f);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[2].q(), 1.f);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[3].q(), 1.f);
  ASSERT_EQ(state_received.type_specific().equalizer().bands_state()[4].q(), 1.f);

  // Not enabled, this is ok, by default they are enabled.
  ASSERT_FALSE(state_received.type_specific().equalizer().bands_state()[0].has_enabled());
  ASSERT_FALSE(state_received.type_specific().equalizer().bands_state()[1].has_enabled());
  ASSERT_FALSE(state_received.type_specific().equalizer().bands_state()[2].has_enabled());
  ASSERT_FALSE(state_received.type_specific().equalizer().bands_state()[3].has_enabled());
  ASSERT_FALSE(state_received.type_specific().equalizer().bands_state()[4].has_enabled());
}

TEST(Tas58xxTest, WatchEqualizerUpdates) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs one AGL and one EQUALIZER.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[0].type(),
            signal_fidl::ElementType::AUTOMATIC_GAIN_LIMITER);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  signal_fidl::ElementState state_received;
  signal_processing_client->WatchElementState(result.response().processing_elements[1].id(),
                                              &state_received);
  ASSERT_TRUE(state_received.has_enabled());
  ASSERT_TRUE(state_received.enabled());
  ASSERT_TRUE(state_received.has_type_specific());
  ASSERT_TRUE(state_received.type_specific().is_equalizer());
  ASSERT_TRUE(state_received.type_specific().equalizer().has_bands_state());

  // A Watch after a SetPE disable must reply since the PE state changed.
  {
    // Control the EQ by disable the whole processing element.
    mock_i2c.ExpectWriteStop({0x66, 0x07});  // Enable bypass EQ.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(false);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());

    signal_fidl::ElementState state_received;
    signal_processing_client->WatchElementState(result.response().processing_elements[1].id(),
                                                &state_received);
    ASSERT_TRUE(state_received.has_enabled());
    ASSERT_FALSE(state_received.enabled());
  }

  // A Watch potentially before a SetPE disable must reply since the PE state changed.
  {
    std::thread th([&]() {
      signal_fidl::ElementState state_received;
      signal_processing_client->WatchElementState(result.response().processing_elements[1].id(),
                                                  &state_received);
      ASSERT_TRUE(state_received.has_enabled());
      ASSERT_FALSE(state_received.enabled());
    });
    // Not required for the test to pass, but rather makes it likely for the watch to start before
    // the SetPE, either way the test is valid.
    zx::nanosleep(zx::deadline_after(zx::msec(10)));

    // Control the EQ by disable the whole processing element.
    mock_i2c.ExpectWriteStop({0x66, 0x07});  // Enable bypass EQ.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(false);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());

    th.join();
  }
}

TEST(Tas58xxTest, SetEqualizerBandDisabled) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support and its parameters.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);
  ASSERT_EQ(result.response().processing_elements[1].type_specific().equalizer().min_frequency(),
            100);
  ASSERT_EQ(result.response().processing_elements[1].type_specific().equalizer().max_frequency(),
            20'000);
  ASSERT_EQ(result.response().processing_elements[1].type_specific().equalizer().min_gain_db(),
            -5.f);
  ASSERT_EQ(result.response().processing_elements[1].type_specific().equalizer().max_gain_db(),
            5.f);
  ASSERT_EQ(
      result.response().processing_elements[1].type_specific().equalizer().supported_controls(),
      signal_fidl::EqualizerSupportedControls::SUPPORTS_TYPE_PEAK |
          signal_fidl::EqualizerSupportedControls::CAN_CONTROL_FREQUENCY);

  // Control the EQ by disable the first band.

  mock_i2c.ExpectWriteStop({0x66, 0x06});  // Disable bypass EQ since PE is enabled.

  // We expect reset of the hardware parameters for the band.
  mock_i2c
      .ExpectWriteStop({0x00, 0x00})    // page 0.
      .ExpectWriteStop({0x7f, 0xaa})    // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})    // page 0x24.
      .ExpectWriteStop({0x18,           // address 0x18.
                        0x08, 0, 0, 0,  // 0x08, 0, 0, 0 = gain 0.dB.
                        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x26})  // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop(
          {0x40, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // address 0x40.
           0,    0,    0, 0, 0, 0, 0, 0, 0, 0})    // 0x08, 0, 0, 0 = gain 0.dB (factor 1.0).
      .ExpectWriteStop({0x00, 0x00})               // page 0.
      .ExpectWriteStop({0x7f, 0x00});              // book 0.

  // Now we send the EQ control disabling the first band.
  signal_fidl::SignalProcessing_SetElementState_Result result_enable;
  signal_fidl::ElementState control;
  control.set_enabled(true);
  std::vector<signal_fidl::EqualizerBandState> bands_control;
  signal_fidl::EqualizerBandState band_control;
  auto band_id = result.response()
                     .processing_elements[1]
                     .type_specific()
                     .equalizer()
                     .bands()[0]  // We control the band at index 0.
                     .id();
  band_control.set_id(band_id);
  band_control.set_enabled(false);
  bands_control.emplace_back(std::move(band_control));
  signal_fidl::EqualizerElementState eq_control;
  eq_control.set_bands_state(std::move(bands_control));
  auto control_params = signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
  control.set_type_specific(std::move(control_params));
  ASSERT_OK(signal_processing_client->SetElementState(result.response().processing_elements[1].id(),
                                                      std::move(control), &result_enable));
  ASSERT_FALSE(result_enable.is_err());

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetEqualizerDifferentRequests) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // 1. Band does not have an enabled field. The processing element does, but not the band.
  {
    mock_i2c.ExpectWriteStop({0x66, 0x06});  // Disable bypass EQ since PE is enabled.

    // We expect reset of the hardware parameters for the band since we default to disabled.
    mock_i2c
        .ExpectWriteStop({0x00, 0x00})    // page 0.
        .ExpectWriteStop({0x7f, 0xaa})    // book 0xaa.
        .ExpectWriteStop({0x00, 0x24})    // page 0x24.
        .ExpectWriteStop({0x18,           // address 0x18.
                          0x08, 0, 0, 0,  // 0x08, 0, 0, 0 = gain 0.dB.
                          0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
        .ExpectWriteStop({0x00, 0x26})  // page 0x26, filter used for gain adjustment.
        .ExpectWriteStop(
            {0x40, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // address 0x40.
             0,    0,    0, 0, 0, 0, 0, 0, 0, 0})    // 0x08, 0, 0, 0 = gain 0.dB (factor 1.0).
        .ExpectWriteStop({0x00, 0x00})               // page 0.
        .ExpectWriteStop({0x7f, 0x00});              // book 0.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(true);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    auto band_id = result.response()
                       .processing_elements[1]
                       .type_specific()
                       .equalizer()
                       .bands()[0]  // First band (index 0).
                       .id();
    band_control.set_id(band_id);
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());
  }

  // 2. Control a band with bad request. Band has a bad id.
  {
    mock_i2c.ExpectWriteStop({0x66, 0x06});  // Disable bypass EQ since PE is enabled.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(true);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    band_control.set_enabled(true);
    band_control.set_id(12345);  // Bad id.
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_TRUE(result_enable.is_err());
  }

  // 3. Control a band with bad request. Band control requests an unsupported frequency.
  {
    mock_i2c.ExpectWriteStop({0x66, 0x06});  // Disable bypass EQ since PE is enabled.
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(true);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    auto band_id = result.response()
                       .processing_elements[1]
                       .type_specific()
                       .equalizer()
                       .bands()[0]  // First band (index 0).
                       .id();
    band_control.set_enabled(true);
    band_control.set_id(band_id);
    band_control.set_frequency(96'000);  // Unsupported frequency.
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_TRUE(result_enable.is_err());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetEqualizerBandEnabledWithCodecStarted) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // We expect the start to first go to HiZ then to play mode.
  mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop({0x03, 0x02});
  mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop({0x03, 0x03});

  // We expect the +5dB band control to turn one filter up and the gain compensation down.
  mock_i2c.ExpectWrite({0x03})
      .ExpectReadStop({0x00})
      .ExpectWriteStop({0x03, 0x02})             // Codec is stated, first go to HiZ.
      .ExpectWriteStop({0x66, 0x06})             // Disable bypass EQ.
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x18,                    // address 0x18.
                        0x0e, 0x44, 0x4f, 0x50,  // 0x0e,...gain +5.dB.
                        0xE3, 0xA7, 0x7F, 0xC0,  //
                        0x0E, 0x14, 0xD0, 0x40,  //
                        0x0F, 0xF0, 0xA1, 0x70,  //
                        0xF8, 0x0F, 0x05, 0x10})
      .ExpectWriteStop({0x00, 0x26})             // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop({0x40,                    // address 0x40.
                        0x04, 0x7F, 0xAC, 0xD0,  // 0x04,...gain -5.dB.
                        0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})  // page 0.
      .ExpectWriteStop({0x7f, 0x00})  // book 0.
      .ExpectWrite({0x03})
      .ExpectReadStop({0x00})
      .ExpectWriteStop({0x03, 0x03});  // Codec is stated, now go back to play mode.

  // Start the codec.
  int64_t out_start_time = 0;
  ASSERT_OK(codec_client->Start(&out_start_time));

  // Control the band.
  signal_fidl::SignalProcessing_SetElementState_Result result_enable;
  signal_fidl::ElementState control;
  control.set_enabled(true);
  std::vector<signal_fidl::EqualizerBandState> bands_control;
  signal_fidl::EqualizerBandState band_control;
  auto band_id = result.response()
                     .processing_elements[1]
                     .type_specific()
                     .equalizer()
                     .bands()[0]  // First band (index 0).
                     .id();
  band_control.set_id(band_id);
  band_control.set_enabled(true);
  band_control.set_gain_db(5.f);
  bands_control.emplace_back(std::move(band_control));
  signal_fidl::EqualizerElementState eq_control;
  eq_control.set_bands_state(std::move(bands_control));
  auto control_params = signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
  control.set_type_specific(std::move(control_params));
  ASSERT_OK(signal_processing_client->SetElementState(result.response().processing_elements[1].id(),
                                                      std::move(control), &result_enable));
  ASSERT_FALSE(result_enable.is_err());

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetEqualizer2BandsEnabled) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // For band 1.
  mock_i2c
      .ExpectWriteStop({0x66, 0x06})             // Disable bypass EQ.
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x18,                    // address 0x18.
                        0x09, 0x3a, 0xd0, 0x00,  // 0x09,...gain +1.xxxdB (0x08,... is 0 dB).
                        0xed, 0xa9, 0x81, 0x20,  //
                        0x09, 0x1c, 0x15, 0xd0,  //
                        0x0f, 0xe8, 0x86, 0xd0,  //
                        0xf8, 0x17, 0x1f, 0xe0})
      .ExpectWriteStop({0x00, 0x26})             // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop({0x40,                    // address 0x40.
                        0x06, 0xf0, 0xa9, 0xa0,  // 0x06,...gain -1.xxxdB (0x08,... is 0 dB).
                        0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})   // page 0.
      .ExpectWriteStop({0x7f, 0x00});  // book 0.

  // For band 2.
  mock_i2c
      .ExpectWriteStop({0x66, 0x06})             // Disable bypass EQ.
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x68,                    // address 0x18
                        0x04, 0xfb, 0x4b, 0xd8,  // 0x04, 0xfb,...is almost -3.dB (0x05 is -3.dB).
                        0xff, 0x3a, 0x20, 0x34,  //
                        0x01, 0xac, 0xf0, 0xd8,  //
                        0x01, 0x17, 0x81, 0x38,  //
                        0xfe, 0x98, 0xb3, 0x7a})
      .ExpectWriteStop({0x00, 0x26})  // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop(
          {0x40,                    // address 0x40.
           0x09, 0xcd, 0x9a, 0x40,  // 0x09,... -1.xxxdB = -1.xxxdB from band 1 + +3.dB from band 2.
           0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})   // page 0.
      .ExpectWriteStop({0x7f, 0x00});  // book 0.

  // Control the first band.
  {
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(true);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    auto band_id = result.response()
                       .processing_elements[1]
                       .type_specific()
                       .equalizer()
                       .bands()[0]  // First band (index 0).
                       .id();
    band_control.set_id(band_id);
    band_control.set_enabled(true);
    band_control.set_gain_db(1.2345f);
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));

    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());
  }

  // Control the second band.
  {
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(true);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    auto band_id = result.response()
                       .processing_elements[1]
                       .type_specific()
                       .equalizer()
                       .bands()[4]  // Second band (we choose the index 4).
                       .id();
    band_control.set_id(band_id);
    band_control.set_enabled(true);
    band_control.set_gain_db(-3.f);
    band_control.set_frequency(11'111);
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));

    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetEqualizerOverflows) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // Band setup 1.
  mock_i2c
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x18,                    // address 0x18.
                        0x3b, 0xfe, 0xc6, 0x00,  //
                        0x88, 0xcc, 0xcd, 0x00,  // -14.9dB (-kRegisterMaxIntegerPart), 5.27 format.
                        0x3b, 0x37, 0x0a, 0x80,  //
                        0x0f, 0xfe, 0x24, 0x80,  //
                        0xf8, 0x01, 0x81, 0xc0})
      .ExpectWriteStop({0x00, 0x26})             // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop({0x40,                    // address 0x40.
                        0x00, 0x73, 0x2A, 0xe1,  // 0x00, 0x7....gain close to 0.dB.
                        0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})   // page 0.
      .ExpectWriteStop({0x7f, 0x00});  // book 0.

  // Band setup 2.
  mock_i2c
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x18,                    // address 0x18.
                        0x00, 0x67, 0xd6, 0x17,  // Low gain since we set it to -25dB.
                        0xff, 0x31, 0xb2, 0x08,  //
                        0x00, 0x66, 0x7c, 0x67,  //
                        0x0e, 0x54, 0xab, 0xf0,  //
                        0xf9, 0xab, 0x03, 0xa0})
      .ExpectWriteStop({0x00, 0x26})  // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop(
          {0x40,                    // address 0x40.
           0x77, 0x33, 0x33, 0x00,  // 0x77... gain set to +14.9dB (kRegisterMaxIntegerPart)
           0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})   // page 0.
      .ExpectWriteStop({0x7f, 0x00});  // book 0.

  // Control the first band directly to bypass +-6dB restriction.
  // Setup 1, will overflow in the band configuration.
  codec->SetBand(true, 0, 100, 1.f, 25.);
  // Setup 2, will overflow in the gain adjustment.
  codec->SetBand(true, 0, 100, 1.f, -25.);

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, SetEqualizerElementDisabled) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();

  ddk::CodecProtocolClient codec_proto2(&codec_proto);

  zx::channel channel_remote, channel_local;
  ASSERT_OK(zx::channel::create(0, &channel_local, &channel_remote));
  ddk::CodecProtocolClient proto_client;
  ASSERT_OK(codec_proto2.Connect(std::move(channel_remote)));
  audio_fidl::CodecSyncPtr codec_client;
  codec_client.Bind(std::move(channel_local));

  fidl::InterfaceHandle<signal_fidl::SignalProcessing> signal_processing_handle;
  fidl::InterfaceRequest<signal_fidl::SignalProcessing> signal_processing_request =
      signal_processing_handle.NewRequest();
  ASSERT_OK(codec_client->SignalProcessingConnect(std::move(signal_processing_request)));
  fidl::SynchronousInterfacePtr signal_processing_client = signal_processing_handle.BindSync();

  // We should get 2 PEs, one with AGL and one with EQ support and its parameters.
  signal_fidl::Reader_GetElements_Result result;
  ASSERT_OK(signal_processing_client->GetElements(&result));
  ASSERT_FALSE(result.is_err());
  ASSERT_EQ(result.response().processing_elements.size(), 2);
  ASSERT_EQ(result.response().processing_elements[1].type(), signal_fidl::ElementType::EQUALIZER);

  // 1. Control the EQ by disable the whole processing element.
  mock_i2c.ExpectWriteStop({0x66, 0x07});  // Enable bypass EQ.

  // Now we send the EQ control disabling the processing element.
  {
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(false);
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());
  }

  // 2. Control the EQ by disable the whole processing element, still include configuration for a
  // band.
  mock_i2c
      .ExpectWriteStop({0x66, 0x07})             // Enable bypass EQ.
      .ExpectWriteStop({0x00, 0x00})             // page 0.
      .ExpectWriteStop({0x7f, 0xaa})             // book 0xaa.
      .ExpectWriteStop({0x00, 0x24})             // page 0x24.
      .ExpectWriteStop({0x18,                    // address 0x18.
                        0x0e, 0x44, 0x4f, 0x50,  // 0x0e,...gain +5.dB.
                        0xE3, 0xA7, 0x7F, 0xC0,  //
                        0x0E, 0x14, 0xD0, 0x40,  //
                        0x0F, 0xF0, 0xA1, 0x70,  //
                        0xF8, 0x0F, 0x05, 0x10})
      .ExpectWriteStop({0x00, 0x26})             // page 0x26, filter used for gain adjustment.
      .ExpectWriteStop({0x40,                    // address 0x40.
                        0x04, 0x7F, 0xAC, 0xD0,  // 0x04,...gain -5.dB.
                        0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
      .ExpectWriteStop({0x00, 0x00})   // page 0.
      .ExpectWriteStop({0x7f, 0x00});  // book 0.

  // Now we send the EQ control disabling the processing element.
  {
    signal_fidl::SignalProcessing_SetElementState_Result result_enable;
    signal_fidl::ElementState control;
    control.set_enabled(false);
    std::vector<signal_fidl::EqualizerBandState> bands_control;
    signal_fidl::EqualizerBandState band_control;
    auto band_id = result.response()
                       .processing_elements[1]
                       .type_specific()
                       .equalizer()
                       .bands()[0]  // First band (index 0).
                       .id();
    band_control.set_id(band_id);
    band_control.set_enabled(true);
    band_control.set_gain_db(5.f);
    bands_control.emplace_back(std::move(band_control));
    signal_fidl::EqualizerElementState eq_control;
    eq_control.set_bands_state(std::move(bands_control));
    auto control_params =
        signal_fidl::TypeSpecificElementState::WithEqualizer(std::move(eq_control));
    control.set_type_specific(std::move(control_params));
    ASSERT_OK(signal_processing_client->SetElementState(
        result.response().processing_elements[1].id(), std::move(control), &result_enable));
    ASSERT_FALSE(result_enable.is_err());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, Reset) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    mock_i2c
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x03, 0x02})  // HiZ, Enables DSP.
        .ExpectWriteStop({0x01, 0x11})  // Reset.
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x02, 0x01})  // Normal modulation, mono, no PBTL (Stereo BTL).
        .ExpectWriteStop({0x03, 0x03})  // Play,
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x78, 0x80})  // Clear analog fault.
        .ExpectWriteStop({0x4c, 0x6c})  // digital vol -30dB.
        .ExpectWrite({0x03})
        .ExpectReadStop({0x00})
        .ExpectWriteStop({0x03, 0x08});  // Muted = true.
    ASSERT_OK(client.Reset());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, Bridged) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    // Reset with PBTL mode on.
    mock_i2c
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x03, 0x02})  // HiZ, Enables DSP.
        .ExpectWriteStop({0x01, 0x11})  // Reset.
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x02, 0x05})  // Normal modulation, mono, PBTL (bridged mono).
        .ExpectWriteStop({0x03, 0x03})  // Play,
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x78, 0x80})  // Clear analog fault.
        .ExpectWriteStop({0x4c, 0x6c})  // digital vol -30dB.
        .ExpectWrite({0x03})
        .ExpectReadStop({0x00})
        .ExpectWriteStop({0x03, 0x08});  // Muted = true.
    ASSERT_OK(client.Reset());
  }

  // If bridged, only left channel is ok.
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 1;  // only left channel is ok.
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    mock_i2c.ExpectWriteStop({0x33, 0x00});  // 16 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x00});  // Keep data start sclk.
    format.bits_per_sample = 16;
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    ASSERT_OK(client.SetDaiFormat(std::move(format)));
  }

  // If bridged, right channel is an error.
  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 2;  // right channel is an error.
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 16;
    auto formats = client.GetDaiFormats();
    // Which channel for birdged miode is not checked by IsDaiFormatSupported,
    // so this still returns TRUE.
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    zx::status<CodecFormatInfo> format_info = client.SetDaiFormat(std::move(format));
    EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, format_info.status_value());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, StopStart) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.bridged = true;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    // Reset with PBTL mode on.
    mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop(
        {0x03, 0x02});  // Stop, first go to HiZ.
    mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop(
        {0x03, 0x00});  // Stop, go to deep sleep.
    mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop(
        {0x03, 0x02});  // Start, first go to HiZ.
    mock_i2c.ExpectWrite({0x03}).ExpectReadStop({0x00}).ExpectWriteStop(
        {0x03, 0x03});  // Start, then go back to play mode.
    ASSERT_OK(client.Stop());
    ASSERT_OK(client.Start());
  }

  mock_i2c.VerifyAndClear();
}

TEST(Tas58xxTest, ExternalConfig) {
  auto fake_parent = MockDevice::FakeRootParent();
  mock_i2c::MockI2c mock_i2c;
  mock_i2c.ExpectWrite({0x67}).ExpectReadStop({0x95});  // Check DIE ID.

  metadata::ti::TasConfig metadata = {};
  metadata.number_of_writes1 = 2;
  metadata.init_sequence1[0].address = 0x12;
  metadata.init_sequence1[0].value = 0x34;
  metadata.init_sequence1[1].address = 0x56;
  metadata.init_sequence1[1].value = 0x78;
  metadata.number_of_writes2 = 3;
  metadata.init_sequence2[0].address = 0x11;
  metadata.init_sequence2[0].value = 0x22;
  metadata.init_sequence2[1].address = 0x33;
  metadata.init_sequence2[1].value = 0x44;
  metadata.init_sequence2[2].address = 0x55;
  metadata.init_sequence2[2].value = 0x66;
  fake_parent->SetMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));

  ASSERT_OK(
      SimpleCodecServer::CreateAndAddToDdk<Tas58xxCodec>(fake_parent.get(), mock_i2c.GetProto()));
  auto* child_dev = fake_parent->GetLatestChild();
  ASSERT_NOT_NULL(child_dev);
  auto codec = child_dev->GetDeviceContext<Tas58xxCodec>();
  auto codec_proto = codec->GetProto();
  SimpleCodecClient client;
  client.SetProtocol(&codec_proto);

  {
    // Reset with PBTL mode on.
    mock_i2c
        .ExpectWriteStop({0x12, 0x34})  // External config.
        .ExpectWriteStop({0x56, 0x78})  // External config.
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x02, 0x01})  // Normal modulation, mono, no PBTL (Stereo BTL).
        .ExpectWriteStop({0x03, 0x03})  // Play,
        .ExpectWriteStop({0x00, 0x00})  // Page 0.
        .ExpectWriteStop({0x7f, 0x00})  // book 0.
        .ExpectWriteStop({0x78, 0x80})  // Clear analog fault.
        .ExpectWriteStop({0x4c, 0x6c})  // digital vol -30dB.
        .ExpectWrite({0x03})
        .ExpectReadStop({0x00})
        .ExpectWriteStop({0x03, 0x08});  // Muted = true.
    ASSERT_OK(client.Reset());
  }

  {
    audio::DaiFormat format = {};
    format.number_of_channels = 2;
    format.channels_to_use_bitmask = 3;
    format.sample_format = SampleFormat::PCM_SIGNED;
    format.frame_format = FrameFormat::I2S;
    format.frame_rate = 48000;
    format.bits_per_slot = 32;
    format.bits_per_sample = 32;
    mock_i2c.ExpectWriteStop({0x33, 0x03});  // 32 bits.
    mock_i2c.ExpectWriteStop({0x34, 0x00});  // Keep data start sclk.
    mock_i2c.ExpectWriteStop({0x11, 0x22});  // External config.
    mock_i2c.ExpectWriteStop({0x33, 0x44});  // External config.
    mock_i2c.ExpectWriteStop({0x55, 0x66});  // External config.
    auto formats = client.GetDaiFormats();
    ASSERT_TRUE(IsDaiFormatSupported(format, formats.value()));
    ASSERT_OK(client.SetDaiFormat(std::move(format)));
  }

  mock_i2c.VerifyAndClear();
}

}  // namespace audio
