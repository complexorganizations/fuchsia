// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate contains a type-safe interface to the OpenThread API.
//!
//! This crate assumes that the OpenThread platform interface have been
//! provided externally, perhaps by a separate crate.

#![warn(missing_docs)]
#![warn(missing_debug_implementations)]
#![warn(rust_2018_idioms)]
#![warn(clippy::all)]

pub mod ot;
pub use openthread_sys as otsys;

/// Shorthand for `ot::Box<T>`
pub type OtBox<T> = ot::Box<T>;

/// Shorthand for `ot::Box<ot::Instance>`.
pub type OtInstanceBox = ot::Box<ot::Instance>;

/// Prelude namespace for improving the ergonomics of using this crate.
#[macro_use]
pub mod prelude {
    #![allow(unused_imports)]

    pub use crate::{ot, otsys};
    pub use crate::{OtBox, OtInstanceBox};
    pub use ot::Boxable as _;
    pub use ot::Link as _;
    pub use ot::OtCastable as _;
    pub use ot::Reset as _;
    pub use ot::State as _;
    pub use ot::Tasklets as _;
    pub use ot::Thread as _;

    pub use ot::TaskletsStreamExt as _;
    pub use std::convert::TryFrom as _;
    pub use std::convert::TryInto as _;
}

// Internal prelude namespace for internal crate use only.
#[doc(hidden)]
#[macro_use]
pub(crate) mod prelude_internal {
    #![allow(unused_imports)]

    pub use crate::impl_ot_castable;
    pub use crate::otsys::*;
    pub use crate::prelude::*;
    pub use core::convert::TryFrom;
    pub use core::convert::TryInto;
    pub use futures::prelude::*;
    pub use log::{debug, error, info, trace, warn};
    pub use num::FromPrimitive as _;
    pub use ot::Result;
    pub use ot::{types::*, Boxable, Error, Instance, Link, Platform, Tasklets, Thread};
    pub use static_assertions as sa;
}
