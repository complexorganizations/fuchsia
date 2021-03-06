// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use num_derive::FromPrimitive;

// Magic number for BIND.
pub const BIND_MAGIC_NUM: u32 = 0x42494E44;

// Magic number for SYMB.
pub const SYMB_MAGIC_NUM: u32 = 0x53594E42;

// Magic number for INST.
pub const INSTRUCTION_MAGIC_NUM: u32 = 0x494E5354;

// Magic number for COMP.
pub const COMPOSITE_MAGIC_NUM: u32 = 0x434F4D50;

pub const BYTECODE_VERSION: u32 = 2;

pub const MAX_STRING_LENGTH: usize = 255;

// Bytecode values for enable_debug flag
pub const BYTECODE_ENABLE_DEBUG: u8 = 1;
pub const BYTECODE_DISABLE_DEBUG: u8 = 0;

// The first key in the symbol table. The key increments by 1 with each entry.
pub const SYMB_TBL_START_KEY: u32 = 1;

// Bytecode boolean value for false.
pub const FALSE_VAL: u32 = 0x00;

// Bytecode boolean value for true.
pub const TRUE_VAL: u32 = 0x01;

#[derive(FromPrimitive, PartialEq)]
pub enum RawOp {
    EqualCondition = 0x01,
    InequalCondition = 0x02,
    UnconditionalJump = 0x10,
    JumpIfEqual = 0x11,
    JumpIfNotEqual = 0x12,
    JumpLandPad = 0x20,
    Abort = 0x30,
}

#[derive(Clone, FromPrimitive)]
pub enum RawValueType {
    Key = 0,
    NumberValue,
    StringValue,
    BoolValue,
    EnumValue,
}

#[derive(FromPrimitive, PartialEq)]
pub enum RawNodeType {
    Primary = 0x50,
    Additional = 0x51,
}
