// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <fidl/fuchsia.io/cpp/wire.h>
#include <inttypes.h>
#include <lib/async/dispatcher.h>
#include <lib/fdio/namespace.h>
#include <lib/fdio/vfs.h>
#include <lib/memfs/memfs.h>
#include <lib/sync/completion.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zircon/device/vfs.h>

#include <memory>
#include <utility>

#include <fbl/ref_ptr.h>

#include "src/lib/storage/vfs/cpp/vfs.h"
#include "src/storage/memfs/dnode.h"
#include "src/storage/memfs/memfs.h"
#include "src/storage/memfs/vnode_dir.h"

struct memfs_filesystem {
  std::unique_ptr<memfs::Memfs> vfs;

  explicit memfs_filesystem(std::unique_ptr<memfs::Memfs> fs) : vfs(std::move(fs)) {}
};

zx_status_t memfs_create_filesystem(async_dispatcher_t* dispatcher, memfs_filesystem_t** out_fs,
                                    zx_handle_t* out_root) {
  ZX_DEBUG_ASSERT(dispatcher != nullptr);
  ZX_DEBUG_ASSERT(out_fs != nullptr);
  ZX_DEBUG_ASSERT(out_root != nullptr);

  auto fs_endpoints = fidl::CreateEndpoints<fuchsia_io::Directory>();
  if (!fs_endpoints.is_ok()) {
    return fs_endpoints.status_value();
  }

  std::unique_ptr<memfs::Memfs> vfs;
  fbl::RefPtr<memfs::VnodeDir> root;
  if (zx_status_t status = memfs::Memfs::Create(dispatcher, "<tmp>", &vfs, &root);
      status != ZX_OK) {
    return status;
  }

  std::unique_ptr<memfs_filesystem_t> fs = std::make_unique<memfs_filesystem_t>(std::move(vfs));
  if (zx_status_t status =
          fs->vfs->ServeDirectory(std::move(root), std::move(fs_endpoints->server));
      status != ZX_OK) {
    return status;
  }

  *out_fs = fs.release();
  *out_root = fs_endpoints->client.TakeHandle().release();
  return ZX_OK;
}

zx_status_t memfs_install_at(async_dispatcher_t* dispatcher, const char* path,
                             memfs_filesystem_t** out_fs) {
  fdio_ns_t* ns;
  zx_status_t status = fdio_ns_get_installed(&ns);
  if (status != ZX_OK) {
    return status;
  }

  memfs_filesystem_t* fs;
  zx_handle_t root;
  status = memfs_create_filesystem(dispatcher, &fs, &root);
  if (status != ZX_OK) {
    return status;
  }

  status = fdio_ns_bind(ns, path, root);
  if (status != ZX_OK) {
    memfs_free_filesystem(fs, nullptr);
    return status;
  }

  if (out_fs != nullptr) {
    *out_fs = fs;
  }

  return ZX_OK;
}

zx_status_t memfs_uninstall_unsafe(memfs_filesystem_t* fs, const char* path) {
  fdio_ns_t* ns;
  zx_status_t status = fdio_ns_get_installed(&ns);
  if (status != ZX_OK) {
    return status;
  }

  status = fdio_ns_unbind(ns, path);
  if (status != ZX_OK) {
    return status;
  }

  delete fs;
  return ZX_OK;
}

void memfs_free_filesystem(memfs_filesystem_t* fs, sync_completion_t* unmounted) {
  ZX_DEBUG_ASSERT(fs != nullptr);
  fs->vfs->Shutdown([fs, unmounted](zx_status_t status) {
    delete fs;
    if (unmounted) {
      sync_completion_signal(unmounted);
    }
  });
}
