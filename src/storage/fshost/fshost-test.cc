// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/fshost/llcpp/fidl.h>
#include <fuchsia/io/c/fidl.h>
#include <fuchsia/process/lifecycle/llcpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/namespace.h>
#include <lib/fidl-async/bind.h>
#include <lib/fidl/llcpp/server.h>
#include <lib/zx/channel.h>
#include <zircon/fidl.h>

#include <memory>

#include <cobalt-client/cpp/collector.h>
#include <cobalt-client/cpp/in_memory_logger.h>
#include <fbl/algorithm.h>
#include <fbl/ref_ptr.h>
#include <fs/pseudo_dir.h>
#include <zxtest/zxtest.h>

#include "fs-manager.h"
#include "fs/synchronous_vfs.h"
#include "fshost-fs-provider.h"
#include "metrics.h"
#include "registry.h"
#include "registry_vnode.h"
#include "src/storage/fshost/block-watcher.h"

namespace devmgr {
namespace {

std::unique_ptr<cobalt_client::Collector> MakeCollector() {
  return std::make_unique<cobalt_client::Collector>(
      std::make_unique<cobalt_client::InMemoryLogger>());
}

// Test that when no filesystems have been added to the fshost vnode, it
// stays empty.
TEST(VnodeTestCase, NoFilesystems) {
  async::Loop loop(&kAsyncLoopConfigNoAttachToCurrentThread);

  auto dir = fbl::MakeRefCounted<fs::PseudoDir>();
  auto fshost_vn = fbl::MakeRefCounted<fshost::RegistryVnode>(loop.dispatcher(), dir);

  fbl::RefPtr<fs::Vnode> node;
  EXPECT_EQ(ZX_ERR_NOT_FOUND, dir->Lookup("0", &node));
}

// Test that when filesystem has been added to the fshost vnode, it appears
// in the supplied remote tracking directory.
TEST(VnodeTestCase, AddFilesystem) {
  async::Loop loop(&kAsyncLoopConfigNoAttachToCurrentThread);

  auto dir = fbl::MakeRefCounted<fs::PseudoDir>();
  auto fshost_vn = fbl::MakeRefCounted<fshost::RegistryVnode>(loop.dispatcher(), dir);

  // Adds a new filesystem to the fshost service node.
  // This filesystem should appear as a new entry within |dir|.
  auto endpoints = fidl::CreateEndpoints<::llcpp::fuchsia::io::Directory>();
  ASSERT_OK(endpoints.status_value());

  fidl::UnownedClientEnd<::llcpp::fuchsia::io::Directory> client_value = endpoints->client.borrow();
  ASSERT_OK(fshost_vn->AddFilesystem(std::move(endpoints->client)));
  fbl::RefPtr<fs::Vnode> node;
  ASSERT_OK(dir->Lookup("0", &node));
  EXPECT_EQ(node->GetRemote(), client_value);
}

TEST(VnodeTestCase, AddFilesystemThroughFidl) {
  async::Loop loop(&kAsyncLoopConfigNoAttachToCurrentThread);
  ASSERT_EQ(loop.StartThread(), ZX_OK);

  // set up registry service
  auto registry_endpoints = fidl::CreateEndpoints<::llcpp::fuchsia::fshost::Registry>();
  ASSERT_OK(registry_endpoints.status_value());
  auto [registry_client, registry_server] = std::move(registry_endpoints.value());

  auto dir = fbl::MakeRefCounted<fs::PseudoDir>();
  auto fshost_vn = std::make_unique<fshost::RegistryVnode>(loop.dispatcher(), dir);
  auto server_binding =
      fidl::BindServer(loop.dispatcher(), std::move(registry_server), std::move(fshost_vn));
  ASSERT_TRUE(server_binding.is_ok());

  // make a new "vfs" "client" that doesn't really point anywhere.
  auto vfs_endpoints = fidl::CreateEndpoints<::llcpp::fuchsia::io::Directory>();
  ASSERT_OK(vfs_endpoints.status_value());
  auto [vfs_client, vfs_server] = std::move(vfs_endpoints.value());
  zx_info_handle_basic_t vfs_client_info;
  ASSERT_OK(vfs_client.channel().get_info(ZX_INFO_HANDLE_BASIC, &vfs_client_info,
                                          sizeof(vfs_client_info), nullptr, nullptr));

  // register the filesystem through the fidl interface
  auto resp = ::llcpp::fuchsia::fshost::Registry::Call::RegisterFilesystem(registry_client,
                                                                           std::move(vfs_client));
  ASSERT_TRUE(resp.ok());
  ASSERT_OK(resp.value().s);

  // confirm that the filesystem was registered
  fbl::RefPtr<fs::Vnode> node;
  ASSERT_OK(dir->Lookup("0", &node));
  zx_info_handle_basic_t vfs_remote_info;
  auto remote = node->GetRemote();
  ASSERT_OK(zx_object_get_info(remote.channel(), ZX_INFO_HANDLE_BASIC, &vfs_remote_info,
                               sizeof(vfs_remote_info), nullptr, nullptr));
  EXPECT_EQ(vfs_remote_info.koid, vfs_client_info.koid);
}

// Test that the manager performs the shutdown procedure correctly with respect to externally
// observable behaviors.
TEST(FsManagerTestCase, ShutdownSignalsCompletion) {
  zx::channel dir_request, lifecycle_request;
  FsManager manager(nullptr, std::make_unique<FsHostMetrics>(MakeCollector()));
  Config config;
  BlockWatcher watcher(manager, &config);
  ASSERT_OK(
      manager.Initialize(std::move(dir_request), std::move(lifecycle_request), nullptr, watcher));

  // The manager should not have exited yet: No one has asked for the shutdown.
  EXPECT_FALSE(manager.IsShutdown());

  // Once we trigger shutdown, we expect a shutdown signal.
  sync_completion_t callback_called;
  manager.Shutdown([callback_called = &callback_called](zx_status_t status) {
    EXPECT_OK(status);
    sync_completion_signal(callback_called);
  });
  manager.WaitForShutdown();
  EXPECT_OK(sync_completion_wait(&callback_called, ZX_TIME_INFINITE));

  // It's an error if shutdown gets called twice, but we expect the callback to still get called
  // with the appropriate error message since the shutdown function has no return value.
  sync_completion_reset(&callback_called);
  manager.Shutdown([callback_called = &callback_called](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_INTERNAL);
    sync_completion_signal(callback_called);
  });
  EXPECT_OK(sync_completion_wait(&callback_called, ZX_TIME_INFINITE));
}

// Test that the manager shuts down the filesystems given a call on the lifecycle channel
TEST(FsManagerTestCase, LifecycleStop) {
  zx::channel dir_request, lifecycle_request, lifecycle;
  zx_status_t status = zx::channel::create(0, &lifecycle_request, &lifecycle);
  ASSERT_OK(status);

  FsManager manager(nullptr, std::make_unique<FsHostMetrics>(MakeCollector()));
  Config config;
  BlockWatcher watcher(manager, &config);
  ASSERT_OK(
      manager.Initialize(std::move(dir_request), std::move(lifecycle_request), nullptr, watcher));

  // The manager should not have exited yet: No one has asked for an unmount.
  EXPECT_FALSE(manager.IsShutdown());

  // Call stop on the lifecycle channel
  llcpp::fuchsia::process::lifecycle::Lifecycle::SyncClient client(std::move(lifecycle));
  auto result = client.Stop();
  ASSERT_OK(result.status());

  // the lifecycle channel should be closed now
  zx_signals_t pending;
  EXPECT_OK(client.channel().wait_one(ZX_CHANNEL_PEER_CLOSED, zx::time::infinite(), &pending));
  EXPECT_TRUE(pending & ZX_CHANNEL_PEER_CLOSED);

  // Now we expect a shutdown signal.
  manager.WaitForShutdown();
}

struct Context {
  uint32_t open_flags;
  uint32_t open_count;
  char path[PATH_MAX + 1];
};

zx_status_t DirectoryOpen(void* ctx, uint32_t flags, uint32_t mode, const char* path_data,
                          size_t path_size, zx_handle_t object) {
  Context* context = reinterpret_cast<Context*>(ctx);
  context->open_flags = flags;
  context->open_count += 1;
  memcpy(context->path, path_data, path_size);
  context->path[path_size] = '\0';
  // Having this handle still open does not spark joy.  Thank it for its
  // service, and then let it go.
  zx_handle_close(object);
  return ZX_OK;
}

const fuchsia_io_DirectoryAdmin_ops_t kDirectoryAdminOps = []() {
  fuchsia_io_DirectoryAdmin_ops_t ops;
  ops.Open = DirectoryOpen;
  return ops;
}();

// Test that asking FshostFsProvider for blobexec opens /fs/blob from the
// current installed namespace with the EXEC right
TEST(FshostFsProviderTestCase, CloneBlobExec) {
  async::Loop loop(&kAsyncLoopConfigNoAttachToCurrentThread);
  ASSERT_EQ(loop.StartThread(), ZX_OK);

  fdio_ns_t* ns;
  ASSERT_OK(fdio_ns_get_installed(&ns));

  // Mock out an object that implements DirectoryOpen and records some state;
  // bind it to the server handle.  Install it at /fs.
  zx::channel client, server;
  ASSERT_OK(zx::channel::create(0, &client, &server));

  Context context = {};
  ASSERT_OK(fidl_bind(loop.dispatcher(), server.release(),
                      reinterpret_cast<fidl_dispatch_t*>(fuchsia_io_DirectoryAdmin_dispatch),
                      &context, &kDirectoryAdminOps));
  fdio_ns_bind(ns, "/fs", client.release());

  // Verify that requesting blobexec gets you the handle at /fs/blob, with the
  // permissions expected.
  FshostFsProvider provider;
  zx::channel blobexec = provider.CloneFs("blobexec");

  // Force a describe call on the target of the Open, to resolve the Open.  We
  // expect this to fail because our mock just closes the channel after Open.
  int fd;
  EXPECT_EQ(ZX_ERR_PEER_CLOSED, fdio_fd_create(blobexec.release(), &fd));

  EXPECT_EQ(1, context.open_count);
  uint32_t expected_flags = ZX_FS_RIGHT_READABLE | ZX_FS_RIGHT_WRITABLE | ZX_FS_RIGHT_EXECUTABLE |
                            ZX_FS_RIGHT_ADMIN | ZX_FS_FLAG_DIRECTORY | ZX_FS_FLAG_NOREMOTE;
  EXPECT_EQ(expected_flags, context.open_flags);
  EXPECT_EQ(0, strcmp("blob", context.path));

  // Tear down.
  ASSERT_OK(fdio_ns_unbind(ns, "/fs"));
}

}  // namespace
}  // namespace devmgr
