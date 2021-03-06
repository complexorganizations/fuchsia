// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {futures::future::pending, tracing::info};

#[fuchsia::main(logging_tags = ["simple_component"])]
/// Simple program that never terminates
async fn main() {
    info!("Child created!");
    pending::<()>().await;
}
