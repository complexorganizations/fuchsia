// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod account;
mod account_manager;
mod account_metadata;
mod constants;
mod disk_management;
mod keys;
mod prototype;
#[cfg(test)]
mod testing;

use anyhow::{Context, Error};
use fidl_fuchsia_identity_account::AccountManagerRequestStream;
use fidl_fuchsia_io::{
    OPEN_FLAG_CREATE, OPEN_FLAG_DIRECTORY, OPEN_RIGHT_READABLE, OPEN_RIGHT_WRITABLE,
};
use fuchsia_async as fasync;
use fuchsia_component::server::ServiceFs;
use futures::StreamExt;
use io_util::directory::open_in_namespace;
use log::info;

use crate::account_manager::AccountManager;
use crate::account_metadata::DataDirAccountMetadataStore;
use crate::disk_management::DevDiskManager;
use crate::prototype::NullKeyDerivation;

enum Services {
    AccountManager(AccountManagerRequestStream),
}

#[fasync::run_singlethreaded]
async fn main() -> Result<(), Error> {
    fuchsia_syslog::init_with_tags(&["auth"]).expect("Can't init logger");
    info!("Starting password authenticator");

    let dev_root = open_in_namespace("/dev", OPEN_RIGHT_READABLE | OPEN_RIGHT_WRITABLE)?;
    let disk_manager = DevDiskManager::new(dev_root);

    let metadata_root = open_in_namespace(
        "/data/accounts",
        OPEN_RIGHT_READABLE | OPEN_RIGHT_WRITABLE | OPEN_FLAG_DIRECTORY | OPEN_FLAG_CREATE,
    )?;
    let account_metadata_store = DataDirAccountMetadataStore::new(metadata_root);
    // This will be replaced with a proper key derivation implementation.
    let key_derivation = NullKeyDerivation;
    let account_manager = AccountManager::new(disk_manager, key_derivation, account_metadata_store);

    let mut fs = ServiceFs::new();
    fs.dir("svc").add_fidl_service(Services::AccountManager);
    fs.take_and_serve_directory_handle().context("serving directory handle")?;

    fs.for_each_concurrent(None, |service| match service {
        Services::AccountManager(stream) => account_manager.handle_requests_for_stream(stream),
    })
    .await;

    Ok(())
}
