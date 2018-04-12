// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_BIN_GUEST_MGR_GUEST_MANAGER_IMPL_H_
#define GARNET_BIN_GUEST_MGR_GUEST_MANAGER_IMPL_H_

#include <fuchsia/cpp/guest.h>

#include <unordered_map>

#include "garnet/bin/guest/mgr/guest_environment_impl.h"
#include "lib/app/cpp/application_context.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fxl/macros.h"

namespace guestmgr {

class GuestManagerImpl : public guest::GuestManager {
 public:
  GuestManagerImpl();
  ~GuestManagerImpl() override;

 private:
  // |guest::GuestManager|
  void CreateEnvironment(
      fidl::StringPtr label,
      fidl::InterfaceRequest<guest::GuestEnvironment> env) override;
  void ListGuests(ListGuestsCallback callback) override;
  void Connect(
      uint32_t guest_id,
      ::fidl::InterfaceRequest<guest::GuestController> controller) override;

  std::unique_ptr<component::ApplicationContext> context_;
  fidl::BindingSet<guest::GuestManager> bindings_;
  std::unordered_map<GuestEnvironmentImpl*,
                     std::unique_ptr<GuestEnvironmentImpl>>
      environments_;

  FXL_DISALLOW_COPY_AND_ASSIGN(GuestManagerImpl);
};

}  // namespace guestmgr

#endif  // GARNET_BIN_GUEST_MGR_GUEST_MANAGER_IMPL_H_
