// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUNTESTS_UTILS_SERVICE_PROXY_DIR_H_
#define RUNTESTS_UTILS_SERVICE_PROXY_DIR_H_

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fbl/ref_ptr.h>

#include "src/lib/storage/vfs/cpp/service.h"
#include "src/lib/storage/vfs/cpp/vfs_types.h"

namespace runtests {

// A directory-like object that proxies connection to the underlying
// directory but allows replacing some entries.
class ServiceProxyDir : public fs::Vnode {
 public:
  explicit ServiceProxyDir(fidl::ClientEnd<fuchsia_io::Directory> proxy_dir);

  void AddEntry(std::string name, fbl::RefPtr<fs::Vnode> node);

  // Overridden from |fs::Vnode|:

  fs::VnodeProtocolSet GetProtocols() const final;
  zx_status_t Lookup(std::string_view name, fbl::RefPtr<fs::Vnode>* out) final;
  zx_status_t GetAttributes(fs::VnodeAttributes* a) final;
  zx_status_t GetNodeInfoForProtocol(fs::VnodeProtocol protocol, fs::Rights rights,
                                     fs::VnodeRepresentation* info) final;

 private:
  const fidl::ClientEnd<fuchsia_io::Directory> proxy_dir_;
  std::mutex lock_;
  std::unordered_map<std::string, fbl::RefPtr<fs::Vnode>> entries_ __TA_GUARDED(lock_);
};

}  // namespace runtests

#endif  // RUNTESTS_UTILS_SERVICE_PROXY_DIR_H_
