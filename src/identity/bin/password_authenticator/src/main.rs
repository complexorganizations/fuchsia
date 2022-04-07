// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod account;
mod account_manager;
mod account_metadata;
mod constants;
mod disk_management;
mod insecure;
mod keys;
mod options;
mod pinweaver;
mod scrypt;
#[cfg(test)]
mod testing;

use anyhow::{Context, Error};
use fidl_fuchsia_identity_account::AccountManagerRequestStream;
use fidl_fuchsia_io as fio;
use fuchsia_async as fasync;
use fuchsia_component::server::ServiceFs;
use futures::StreamExt;
use io_util::directory::open_in_namespace;
use log::{error, info};

use crate::{
    account_manager::{AccountManager, EnvCredManagerProvider},
    account_metadata::DataDirAccountMetadataStore,
    disk_management::DevDiskManager,
    options::Options,
};

enum Services {
    AccountManager(AccountManagerRequestStream),
}

#[fasync::run_singlethreaded]
async fn main() -> Result<(), Error> {
    fuchsia_syslog::init_with_tags(&["auth"]).expect("Can't init logger");
    info!("Starting password authenticator");

    let options = argh::from_env::<Options>();
    info!("Command line options = {:?}", options);
    options.validate().map_err(|err| {
        error!("Failed to validate command line options: {:?}", err);
        err
    })?;

    let dev_root =
        open_in_namespace("/dev", fio::OpenFlags::RIGHT_READABLE | fio::OpenFlags::RIGHT_WRITABLE)?;
    let disk_manager = DevDiskManager::new(dev_root);

    let metadata_root = open_in_namespace(
        "/data/accounts",
        fio::OpenFlags::RIGHT_READABLE
            | fio::OpenFlags::RIGHT_WRITABLE
            | fio::OpenFlags::DIRECTORY
            | fio::OpenFlags::CREATE,
    )?;
    let mut account_metadata_store = DataDirAccountMetadataStore::new(metadata_root);
    // Clean up any not-committed files laying around in the account metadata directory.
    let cleanup_res = account_metadata_store.cleanup_stale_files().await;
    // If any cleanup fails, ignore it -- we can still perform our primary function with
    // stale files laying around.
    // TODO(zarvox): someday, make an inspect entry for this failure mode
    drop(cleanup_res);

    let cred_manager_provider = EnvCredManagerProvider {};
    let account_manager =
        AccountManager::new(options, disk_manager, account_metadata_store, cred_manager_provider);

    let mut fs = ServiceFs::new();
    fs.dir("svc").add_fidl_service(Services::AccountManager);
    fs.take_and_serve_directory_handle().context("serving directory handle")?;

    fs.for_each_concurrent(None, |service| match service {
        Services::AccountManager(stream) => account_manager.handle_requests_for_stream(stream),
    })
    .await;

    Ok(())
}
