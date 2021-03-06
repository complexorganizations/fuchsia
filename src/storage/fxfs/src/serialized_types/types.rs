// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::{
    lsm_tree::LayerInfo,
    object_store::{
        transaction::Mutation, AllocatorInfo, AllocatorKey, AllocatorValue, EncryptedMutations,
        JournalRecord, ObjectKey, ObjectValue, StoreInfo, SuperBlock, SuperBlockRecord,
    },
    serialized_types::{versioned_type, Version, Versioned, VersionedLatest},
};

/// The latest version of on-disk filesystem format.
///
/// If all layer files are compacted the the journal flushed, and super-block
/// both rewritten, all versions should match this value.
///
/// If making a breaking change, please see EARLIEST_SUPPORTED_VERSION (below).
pub const LATEST_VERSION: Version = Version { major: 22, minor: 0 };

/// The earliest supported version of the on-disk filesystem format.
///
/// When a breaking change is made:
/// 1) LATEST_VERSION should have it's major component increased (see above).
/// 2) EARLIEST_SUPPORTED_VERSION should be set to the new LATEST_VERSION.
/// 3) The SuperBlock version (below) should also be set to the new LATEST_VERSION.
pub const EARLIEST_SUPPORTED_VERSION: Version = Version { major: 21, minor: 0 };

versioned_type! {
    18.. => AllocatorInfo,
}
versioned_type! {
    1.. => AllocatorKey,
}
versioned_type! {
    12.. => AllocatorValue,
}
versioned_type! {
    5.. => EncryptedMutations,
}
versioned_type! {
    20.. => JournalRecord,
}
versioned_type! {
    1.. => LayerInfo,
}
versioned_type! {
    20.. => Mutation,
}
versioned_type! {
    5.. => ObjectKey,
}
versioned_type! {
    5.. => ObjectValue,
}
versioned_type! {
    17.. => StoreInfo,
}
versioned_type! {
    21.. => SuperBlock,
}
versioned_type! {
    5.. => SuperBlockRecord,
}
