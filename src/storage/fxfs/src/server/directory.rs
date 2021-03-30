// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::{
        errors::FxfsError,
        object_store::{self, transaction::Transaction},
        server::{errors::fxfs_error, node::FxNode, volume::FxVolume},
    },
    anyhow::{bail, Error},
    async_trait::async_trait,
    fidl::endpoints::ServerEnd,
    fidl_fuchsia_io::{
        self as fio, NodeAttributes, NodeMarker, MODE_TYPE_DIRECTORY, OPEN_FLAG_CREATE,
        OPEN_FLAG_CREATE_IF_ABSENT,
    },
    fuchsia_async as fasync,
    fuchsia_zircon::Status,
    std::{any::Any, sync::Arc},
    vfs::{
        common::send_on_open_with_error,
        directory::{
            connection::{io1::DerivedConnection, util::OpenDirectory},
            dirents_sink::{self, Sink},
            entry::{DirectoryEntry, EntryInfo},
            entry_container::{AsyncGetEntry, Directory, MutableDirectory},
            traversal_position::TraversalPosition,
        },
        execution_scope::ExecutionScope,
        filesystem::Filesystem,
        path::Path,
    },
};

pub struct FxDirectory {
    volume: Arc<FxVolume>,
    directory: object_store::Directory,
}

impl FxDirectory {
    pub(crate) fn new(volume: Arc<FxVolume>, directory: object_store::Directory) -> Self {
        Self { volume, directory }
    }

    async fn lookup(
        self: &Arc<Self>,
        flags: u32,
        mode: u32,
        mut path: Path,
    ) -> Result<FxNode, Error> {
        let mut current_node = FxNode::Dir(self.clone());
        while !path.is_empty() {
            let last_segment = path.is_single_component();
            let current_dir = match current_node {
                FxNode::Dir(dir) => dir.clone(),
            };
            let name = path.next().unwrap();
            match current_dir.directory.lookup(name).await {
                Ok((object_id, _object_type)) => {
                    if last_segment
                        && flags & OPEN_FLAG_CREATE != 0
                        && flags & OPEN_FLAG_CREATE_IF_ABSENT != 0
                    {
                        return Err(FxfsError::AlreadyExists.into());
                    }
                    // TODO(jfsulliv) this might not be loaded from disk yet.
                    current_node =
                        self.volume.open_node(object_id).map_err(|_| FxfsError::Inconsistent)?;
                }
                Err(e) => match e.downcast_ref::<FxfsError>() {
                    Some(FxfsError::NotFound) => {
                        if last_segment && flags & OPEN_FLAG_CREATE != 0 {
                            return self.create_node(&current_dir, name, mode).await;
                        } else {
                            return Err(FxfsError::NotFound.into());
                        }
                    }
                    _ => return Err(e),
                },
            }
        }
        Ok(current_node)
    }

    async fn create_node(
        self: &Arc<Self>,
        dir: &Arc<FxDirectory>,
        name: &str,
        mode: u32,
    ) -> Result<FxNode, Error> {
        let mut transaction = Transaction::new();
        let (object_id, node) = if mode & MODE_TYPE_DIRECTORY != 0 {
            let dir = dir.directory.create_child_dir(&mut transaction, name).await?;
            let object_id = dir.object_id();
            let node =
                FxNode::Dir(Arc::new(FxDirectory { volume: self.volume.clone(), directory: dir }));
            (object_id, node)
        } else {
            bail!("Files not implemented yet");
        };
        self.volume.store().filesystem().commit_transaction(transaction).await;
        self.volume.add_node(object_id, node)
    }
}

#[async_trait]
impl MutableDirectory for FxDirectory {
    async fn link(&self, _name: String, _entry: Arc<dyn DirectoryEntry>) -> Result<(), Status> {
        log::error!("link not implemented");
        Err(Status::NOT_SUPPORTED)
    }

    async fn unlink(&self, _path: Path) -> Result<(), Status> {
        log::error!("unlink not implemented");
        Err(Status::NOT_SUPPORTED)
    }

    async fn set_attrs(&self, _flags: u32, _attrs: NodeAttributes) -> Result<(), Status> {
        log::error!("set_attrs not implemented");
        Err(Status::NOT_SUPPORTED)
    }

    fn get_filesystem(&self) -> &dyn Filesystem {
        &*self.volume
    }

    fn into_any(self: Arc<Self>) -> Arc<dyn Any + Sync + Send> {
        self as Arc<dyn Any + Sync + Send>
    }

    async fn sync(&self) -> Result<(), Status> {
        // TODO(csuter): Support sync on root of fxfs volume.
        Ok(())
    }
}

impl DirectoryEntry for FxDirectory {
    fn open(
        self: Arc<Self>,
        scope: ExecutionScope,
        flags: u32,
        mode: u32,
        path: Path,
        server_end: ServerEnd<NodeMarker>,
    ) {
        let cloned_scope = scope.clone();
        scope.spawn(async move {
            match self.lookup(flags, mode, path).await {
                Err(e) => send_on_open_with_error(flags, server_end, fxfs_error(e).into()),
                Ok(FxNode::Dir(dir)) => {
                    vfs::directory::mutable::connection::io1::MutableConnection::create_connection(
                        cloned_scope,
                        OpenDirectory::new(dir.clone()),
                        flags,
                        mode,
                        server_end,
                    );
                }
            };
        });
    }

    fn entry_info(&self) -> EntryInfo {
        EntryInfo::new(fio::INO_UNKNOWN, fio::DIRENT_TYPE_DIRECTORY)
    }

    fn can_hardlink(&self) -> bool {
        false
    }
}

#[async_trait]
impl Directory for FxDirectory {
    fn get_entry(self: Arc<Self>, _name: String) -> AsyncGetEntry {
        // TODO(jfsulliv): Implement
        AsyncGetEntry::Immediate(Err(Status::NOT_FOUND))
    }

    async fn read_dirents<'a>(
        &'a self,
        _pos: &'a TraversalPosition,
        sink: Box<dyn Sink>,
    ) -> Result<(TraversalPosition, Box<dyn dirents_sink::Sealed>), Status> {
        // TODO(jfsulliv): Implement
        Ok((TraversalPosition::End, sink.seal()))
    }

    fn register_watcher(
        self: Arc<Self>,
        _scope: ExecutionScope,
        _mask: u32,
        _channel: fasync::Channel,
    ) -> Result<(), Status> {
        // TODO(jfsulliv): Implement
        Err(Status::NOT_SUPPORTED)
    }

    fn unregister_watcher(self: Arc<Self>, _key: usize) {
        // TODO(jfsulliv): Implement
    }

    async fn get_attrs(&self) -> Result<NodeAttributes, Status> {
        Err(Status::NOT_SUPPORTED)
    }

    fn close(&self) -> Result<(), Status> {
        // TODO(jfsulliv): Implement
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use {
        crate::{
            object_store::FxFilesystem,
            server::{
                testing::{open_dir_validating, open_file},
                volume::FxVolumeAndRoot,
            },
            testing::fake_device::FakeDevice,
            volume::volume_directory,
        },
        anyhow::Error,
        fidl::endpoints::ServerEnd,
        fidl_fuchsia_io::{
            DirectoryMarker, MODE_TYPE_DIRECTORY, MODE_TYPE_FILE, OPEN_FLAG_CREATE,
            OPEN_FLAG_DIRECTORY, OPEN_RIGHT_READABLE, OPEN_RIGHT_WRITABLE,
        },
        fuchsia_async as fasync,
        fuchsia_zircon::Status,
        matches::assert_matches,
        std::sync::Arc,
        vfs::{directory::entry::DirectoryEntry, execution_scope::ExecutionScope, path::Path},
    };

    #[fasync::run_singlethreaded(test)]
    async fn test_open_root_dir() -> Result<(), Error> {
        let device = Arc::new(FakeDevice::new(2048, 512));
        let filesystem = FxFilesystem::new_empty(device.clone()).await?;
        let volume_directory = volume_directory(&filesystem).await?;
        let vol = FxVolumeAndRoot::new(volume_directory.new_volume("vol").await?);
        let dir = vol.root().clone();

        let (dir_proxy, dir_server_end) =
            fidl::endpoints::create_proxy::<DirectoryMarker>().expect("Create proxy to succeed");

        dir.open(
            ExecutionScope::new(),
            OPEN_FLAG_DIRECTORY | OPEN_RIGHT_READABLE,
            MODE_TYPE_DIRECTORY,
            Path::empty(),
            ServerEnd::new(dir_server_end.into_channel()),
        );

        dir_proxy.describe().await.expect("Describe to succeed");

        Ok(())
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_open_nonexistent_file() -> Result<(), Error> {
        let device = Arc::new(FakeDevice::new(2048, 512));
        let filesystem = FxFilesystem::new_empty(device.clone()).await?;
        let volume_directory = volume_directory(&filesystem).await?;
        let vol = FxVolumeAndRoot::new(volume_directory.new_volume("vol").await?);
        let dir = vol.root().clone();

        let (dir_proxy, dir_server_end) =
            fidl::endpoints::create_proxy::<DirectoryMarker>().expect("Create proxy to succeed");

        dir.open(
            ExecutionScope::new(),
            OPEN_FLAG_DIRECTORY | OPEN_RIGHT_READABLE,
            MODE_TYPE_DIRECTORY,
            Path::empty(),
            ServerEnd::new(dir_server_end.into_channel()),
        );

        let child_proxy = open_file(&dir_proxy, OPEN_RIGHT_READABLE, MODE_TYPE_FILE, "foo")
            .expect("Create proxy failed");

        // The channel also be closed with a NOT_FOUND epitaph.
        assert_matches!(
            child_proxy.describe().await,
            Err(fidl::Error::ClientChannelClosed {
                status: Status::NOT_FOUND,
                service_name: "(anonymous) File",
            })
        );

        Ok(())
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_create_dir_nested() -> Result<(), Error> {
        let device = Arc::new(FakeDevice::new(2048, 512));
        let filesystem = FxFilesystem::new_empty(device.clone()).await?;
        let volume_directory = volume_directory(&filesystem).await?;
        let vol = FxVolumeAndRoot::new(volume_directory.new_volume("vol").await?);
        let dir = vol.root().clone();

        let (dir_proxy, dir_server_end) =
            fidl::endpoints::create_proxy::<DirectoryMarker>().expect("Create proxy to succeed");

        dir.open(
            ExecutionScope::new(),
            OPEN_FLAG_DIRECTORY | OPEN_RIGHT_READABLE | OPEN_RIGHT_WRITABLE,
            MODE_TYPE_DIRECTORY,
            Path::empty(),
            ServerEnd::new(dir_server_end.into_channel()),
        );

        let subdir_proxy = open_dir_validating(
            &dir_proxy,
            OPEN_FLAG_CREATE | OPEN_RIGHT_READABLE | OPEN_RIGHT_WRITABLE,
            MODE_TYPE_DIRECTORY,
            "foo",
        )
        .await
        .expect("Create dir failed");

        open_dir_validating(
            &subdir_proxy,
            OPEN_FLAG_CREATE | OPEN_RIGHT_READABLE,
            MODE_TYPE_DIRECTORY,
            "bar",
        )
        .await
        .expect("Create nested dir failed");

        // Also make sure we can follow the path.
        open_dir_validating(&dir_proxy, OPEN_RIGHT_READABLE, MODE_TYPE_DIRECTORY, "foo/bar")
            .await
            .expect("Open nested dir failed");

        Ok(())
    }
}
