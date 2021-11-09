// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_H_
#define LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_H_

#include <lib/fidl/coding.h>
#include <zircon/assert.h>
#include <zircon/fidl.h>

#include <cstdint>
#include <type_traits>

namespace fidl {

// Options passed from the user-facing write API to transport write().
struct WriteOptions {};

// Options passed from the user-facing read API to transport read().
struct ReadOptions {
  bool discardable = false;
};

// Options passed from the user-facing call API to transport call().
struct CallOptions {
  zx_time_t deadline = ZX_TIME_INFINITE;
};

namespace internal {

struct CallMethodArgs {
  const void* wr_data;
  const fidl_handle_t* wr_handles;
  const void* wr_handle_metadata;
  uint32_t wr_data_count;
  uint32_t wr_handles_count;

  void* rd_data;
  fidl_handle_t* rd_handles;
  void* rd_handle_metadata;
  uint32_t rd_data_capacity;
  uint32_t rd_handles_capacity;
};

// An instance of TransportVTable contains function definitions to implement transport-specific
// functionality.
struct TransportVTable {
  fidl_transport_type type;
  const CodingConfig* encoding_configuration;

  // Write to the transport.
  // |handle_metadata| contains transport-specific metadata produced by
  // EncodingConfiguration::decode_process_handle.
  zx_status_t (*write)(fidl_handle_t handle, WriteOptions options, const void* data,
                       uint32_t data_count, const fidl_handle_t* handles,
                       const void* handle_metadata, uint32_t handles_count);

  // Read from the transport.
  // This populates |handle_metadata|, which contains transport-specific metadata and will be
  // passed to EncodingConfiguration::decode_process_handle.
  zx_status_t (*read)(fidl_handle_t handle, ReadOptions options, void* data, uint32_t data_capacity,
                      fidl_handle_t* handles, void* handle_metadata, uint32_t handles_capacity,
                      uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count);

  // Perform a call on the transport.
  // The arguments are formatted in |cargs|, with the write direction args corresponding to
  // those in |write| and the read direction args corresponding to those in |read|.
  zx_status_t (*call)(fidl_handle_t handle, CallOptions options, const CallMethodArgs& cargs,
                      uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count);

  // Close the handle.
  void (*close)(fidl_handle_t);
};

// A type-erased unowned transport (e.g. generalized zx::unowned_channel).
// Create an |AnyUnownedTransport| object with |MakeAnyUnownedTransport|, implemented for each of
// the transport types.
class AnyUnownedTransport {
 public:
  template <typename Transport>
  static constexpr AnyUnownedTransport Make(fidl_handle_t handle) noexcept {
    return AnyUnownedTransport(&Transport::VTable, handle);
  }

  AnyUnownedTransport(const AnyUnownedTransport&) = default;
  AnyUnownedTransport& operator=(const AnyUnownedTransport&) = default;
  AnyUnownedTransport(AnyUnownedTransport&& other) noexcept = default;
  AnyUnownedTransport& operator=(AnyUnownedTransport&& other) noexcept = default;

  template <typename Transport>
  typename Transport::UnownedType get() const {
    ZX_ASSERT(vtable_->type == Transport::VTable.type);
    return typename Transport::UnownedType(handle_);
  }

  const TransportVTable* vtable() const { return vtable_; }

  fidl_handle_t handle() const { return handle_; }

  fidl_transport_type type() const { return vtable_->type; }

  zx_status_t write(WriteOptions options, const void* data, uint32_t data_count,
                    const fidl_handle_t* handles, const void* handle_metadata,
                    uint32_t handles_count) const {
    return vtable_->write(handle_, options, data, data_count, handles, handle_metadata,
                          handles_count);
  }

  zx_status_t read(ReadOptions options, void* data, uint32_t data_capacity, fidl_handle_t* handles,
                   void* handle_metadata, uint32_t handles_capacity,
                   uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count) const {
    return vtable_->read(handle_, options, data, data_capacity, handles, handle_metadata,
                         handles_capacity, out_data_actual_count, out_handles_actual_count);
  }

  zx_status_t call(CallOptions options, const CallMethodArgs& cargs,
                   uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count) const {
    return vtable_->call(handle_, options, cargs, out_data_actual_count, out_handles_actual_count);
  }

 private:
  friend class AnyTransport;
  explicit constexpr AnyUnownedTransport(const TransportVTable* vtable, fidl_handle_t handle)
      : vtable_(vtable), handle_(handle) {}

  [[maybe_unused]] const TransportVTable* vtable_;
  [[maybe_unused]] fidl_handle_t handle_;
};

// A type-erased owned transport (e.g. generalized zx::channel).
// Create an |AnyTransport| object with |MakeAnyTransport|, implemented for each of
// the transport types.
class AnyTransport {
 public:
  template <typename Transport>
  static AnyTransport Make(fidl_handle_t handle) noexcept {
    return AnyTransport(&Transport::VTable, handle);
  }

  AnyTransport(const AnyTransport&) = delete;
  AnyTransport& operator=(const AnyTransport&) = delete;

  AnyTransport(AnyTransport&& other) noexcept : vtable_(other.vtable_), handle_(other.handle_) {
    other.handle_ = FIDL_HANDLE_INVALID;
  }
  AnyTransport& operator=(AnyTransport&& other) noexcept {
    vtable_ = other.vtable_;
    handle_ = other.handle_;
    other.handle_ = FIDL_HANDLE_INVALID;
    return *this;
  }
  ~AnyTransport() {
    if (handle_ != FIDL_HANDLE_INVALID) {
      vtable_->close(handle_);
    }
  }

  constexpr AnyUnownedTransport borrow() const { return AnyUnownedTransport(vtable_, handle_); }

  template <typename Transport>
  typename Transport::UnownedType get() const {
    ZX_ASSERT(vtable_->type == Transport::VTable.type);
    return typename Transport::UnownedType(handle_);
  }

  template <typename Transport>
  typename Transport::OwnedType release() {
    ZX_ASSERT(vtable_->type == Transport::VTable.type);
    fidl_handle_t temp = handle_;
    handle_ = FIDL_HANDLE_INVALID;
    return typename Transport::OwnedType(temp);
  }

  const TransportVTable* vtable() const { return vtable_; }

  fidl_handle_t handle() const { return handle_; }

  fidl_transport_type type() const { return vtable_->type; }

  zx_status_t write(WriteOptions options, const void* data, uint32_t data_count,
                    const fidl_handle_t* handles, const void* handle_metadata,
                    uint32_t handles_count) const {
    return vtable_->write(handle_, options, data, data_count, handles, handle_metadata,
                          handles_count);
  }

  zx_status_t read(ReadOptions options, void* data, uint32_t data_capacity, fidl_handle_t* handles,
                   void* handle_metadata, uint32_t handles_capacity,
                   uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count) const {
    return vtable_->read(handle_, options, data, data_capacity, handles, handle_metadata,
                         handles_capacity, out_data_actual_count, out_handles_actual_count);
  }

  zx_status_t call(CallOptions options, const CallMethodArgs& cargs,
                   uint32_t* out_data_actual_count, uint32_t* out_handles_actual_count) const {
    return vtable_->call(handle_, options, cargs, out_data_actual_count, out_handles_actual_count);
  }

 private:
  explicit constexpr AnyTransport(const TransportVTable* vtable, fidl_handle_t handle)
      : vtable_(vtable), handle_(handle) {}

  const TransportVTable* vtable_;
  fidl_handle_t handle_;
};

AnyUnownedTransport MakeAnyUnownedTransport(const AnyTransport& transport);

template <typename TransportObject>
struct AssociatedTransportImpl;

template <typename TransportObject>
using AssociatedTransport = typename AssociatedTransportImpl<std::decay_t<TransportObject>>::type;

}  // namespace internal

}  // namespace fidl

#endif  // LIB_FIDL_LLCPP_INTERNAL_TRANSPORT_H_
