// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    anyhow::{self, Error},
    diagnostics_reader::{ArchiveReader, Logs},
    fidl_fuchsia_data as fdata,
    fuchsia_component_test::{ChildOptions, Moniker, RealmBuilder},
    futures::StreamExt,
    std::fs::File,
    std::io::{self, BufRead},
};

#[fuchsia::test]
async fn wisdom_integration_test() -> Result<(), Error> {
    // Create the test realm,
    let builder = RealmBuilder::new().await?;
    builder.add_child(Moniker::root(), "#meta/intl_wisdom_realm.cm", ChildOptions::new()).await?;

    // Mark echo_client as eager so it starts automatically.
    builder.mark_as_eager("wisdom_client").await?;

    // Inject the program.args of the manifest
    let mut client_decl = builder.get_decl("wisdom_client").await?;
    let program = client_decl.program.as_mut().unwrap();
    program.info.entries.as_mut().unwrap().push(fdata::DictionaryEntry {
        key: "args".into(),
        value: Some(Box::new(fdata::DictionaryValue::StrVec(vec![
            "--timestamp=2018-11-01T12:34:56Z".to_string(),
            "--timezone=America/Los_Angeles".to_string(),
        ]))),
    });
    builder.set_decl("wisdom_client", client_decl).await?;

    // Create the realm instance
    let realm_instance = builder.build().await?;

    // Initialize the log reader
    let moniker = format!(
        "fuchsia_component_test_collection\\:{}/wisdom_client",
        realm_instance.root.child_name()
    );
    let mut reader = ArchiveReader::new();
    reader.add_selector(format!("{}:root", moniker));
    let mut log_stream = reader.snapshot_then_subscribe::<Logs>()?;

    // Initialize Goldens file
    let goldens = File::open("/pkg/data/golden-output.txt")?;
    let mut lines = io::BufReader::new(goldens).lines();

    // Verify each line matches the log output
    while let Some(line) = lines.next() {
        let logs = log_stream.next().await.expect("got log result")?;
        assert_eq!(logs.msg().unwrap(), line.unwrap());
    }

    // Clean up the realm instance
    realm_instance.destroy().await?;

    Ok(())
}
