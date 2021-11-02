// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cfg(test)]
pub mod tests {

    use anyhow::Error;
    use device_watcher;
    use fidl_fuchsia_io;
    use fuchsia_component_test::{builder::RealmBuilder, RealmInstance};
    use fuchsia_driver_test::{DriverTestRealmBuilder, DriverTestRealmInstance};

    pub async fn get_dev_proxy() -> Result<(RealmInstance, fidl_fuchsia_io::DirectoryProxy), Error>
    {
        let mut realm = RealmBuilder::new().await?;
        realm.driver_test_realm_setup().await?;
        let instance = realm.build().create().await?;
        instance.driver_test_realm_start(fidl_fuchsia_driver_test::RealmArgs::EMPTY).await?;
        let dev_dir = instance.driver_test_realm_connect_to_dev()?;
        let codecs_dir = io_util::directory::open_directory(
            &dev_dir,
            "class/codec",
            fidl_fuchsia_io::OPEN_RIGHT_READABLE,
        )
        .await?;
        // Wait for the first codec node 000.
        device_watcher::recursive_wait_and_open_node(&codecs_dir, "000").await?;
        Ok((instance, codecs_dir))
    }
}
