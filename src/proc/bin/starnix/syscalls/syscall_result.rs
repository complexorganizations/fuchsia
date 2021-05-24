// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::fs::FdNumber;
use crate::signals::Signal;
use crate::types::{sigaction_t, UserAddress};

/// The result of executing a syscall.
///
/// It would be nice to have this also cover errors, but currently there is no stable way
/// to implement `std::ops::Try` (for the `?` operator) for custom enums, making it difficult
/// to work with.
#[derive(Debug, Eq, PartialEq)]
pub enum SyscallResult {
    /// The process exited as a result of the syscall. The associated `u64` represents the process'
    /// exit code.
    Exit(i32),

    /// The syscall completed successfully. The associated `u64` is the return value from the
    /// syscall.
    Success(u64),

    /// The syscall completed successfully and the handler for `Signal` should be executed.
    HandleSignal(Signal, sigaction_t),
}

pub const SUCCESS: SyscallResult = SyscallResult::Success(0);

impl From<UserAddress> for SyscallResult {
    fn from(value: UserAddress) -> Self {
        SyscallResult::Success(value.ptr() as u64)
    }
}

impl From<FdNumber> for SyscallResult {
    fn from(value: FdNumber) -> Self {
        SyscallResult::Success(value.raw() as u64)
    }
}

impl From<bool> for SyscallResult {
    fn from(value: bool) -> Self {
        SyscallResult::Success(if value { 1 } else { 0 })
    }
}

impl From<i32> for SyscallResult {
    fn from(value: i32) -> Self {
        SyscallResult::Success(value as u64)
    }
}

impl From<u32> for SyscallResult {
    fn from(value: u32) -> Self {
        SyscallResult::Success(value as u64)
    }
}

impl From<u64> for SyscallResult {
    fn from(value: u64) -> Self {
        SyscallResult::Success(value)
    }
}

impl From<usize> for SyscallResult {
    fn from(value: usize) -> Self {
        SyscallResult::Success(value as u64)
    }
}
