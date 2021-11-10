// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS FILE WAS AUTO-GENERATED BY $OUT_DIR/../../src/lib/dynfidl/rust/tests/conformance/test_from_ir/src/main.rs
#![allow(non_snake_case)] // for easy copy/paste from test output to build config
use dynfidl::{Field, Structure};
use fidl::encoding::{decode_persistent, encode_persistent, Persistable};
use std::{cmp::PartialEq, fmt::Debug};
#[track_caller]
fn test_persistent_roundtrip<T: Debug + PartialEq + Persistable>(
    mut domain_value: T,
    structure: Structure,
) {
    let binding_encoded = encode_persistent(&mut domain_value).unwrap();
    let dynamic_encoded = structure.encode_persistent();
    assert_eq!(
        binding_encoded, dynamic_encoded,
        "encoded messages from bindings and dynfidl must match",
    );
    let domain_from_dynamic: T = decode_persistent(&dynamic_encoded[..]).unwrap();
    assert_eq!(
        domain_value, domain_from_dynamic,
        "domain value from dynamic encoding must match original",
    );
}
#[test]
fn roundtrip_persistent_conformance_EmptyStruct() {
    let domain_value = fidl_conformance::EmptyStruct {};
    let dynfidl_structure = Structure::default();
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_FidlvizStruct1() {
    let domain_value = fidl_conformance::FidlvizStruct1 {};
    let dynfidl_structure = Structure::default();
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_FidlvizStruct2() {
    let x = 5u64;
    let domain_value = fidl_conformance::FidlvizStruct2 { x: x.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt64(x));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_FiveByte() {
    let elem1 = 4u32;
    let elem2 = 2u8;
    let domain_value = fidl_conformance::FiveByte { elem1: elem1.clone(), elem2: elem2.clone() };
    let dynfidl_structure =
        Structure::default().field(Field::UInt32(elem1)).field(Field::UInt8(elem2));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_GoldenBoolStruct() {
    let v = true;
    let domain_value = fidl_conformance::GoldenBoolStruct { v: v.clone() };
    let dynfidl_structure = Structure::default().field(Field::Bool(v));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_GoldenIntStruct() {
    let v = 7i16;
    let domain_value = fidl_conformance::GoldenIntStruct { v: v.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(v));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_GoldenUintStruct() {
    let v = 3u16;
    let domain_value = fidl_conformance::GoldenUintStruct { v: v.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt16(v));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Int64Struct() {
    let x = 9i64;
    let domain_value = fidl_conformance::Int64Struct { x: x.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int64(x));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyBool() {
    let value = true;
    let domain_value = fidl_conformance::MyBool { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::Bool(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyByte() {
    let value = 2u8;
    let domain_value = fidl_conformance::MyByte { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt8(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyInt16() {
    let value = 7i16;
    let domain_value = fidl_conformance::MyInt16 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyInt32() {
    let value = 8i32;
    let domain_value = fidl_conformance::MyInt32 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyInt64() {
    let value = 9i64;
    let domain_value = fidl_conformance::MyInt64 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int64(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyInt8() {
    let value = 6i8;
    let domain_value = fidl_conformance::MyInt8 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int8(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyUint16() {
    let value = 3u16;
    let domain_value = fidl_conformance::MyUint16 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt16(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyUint32() {
    let value = 4u32;
    let domain_value = fidl_conformance::MyUint32 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt32(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyUint64() {
    let value = 5u64;
    let domain_value = fidl_conformance::MyUint64 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt64(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_MyUint8() {
    let value = 2u8;
    let domain_value = fidl_conformance::MyUint8 { value: value.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt8(value));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_NodeAttributes() {
    let mode = 4u32;
    let id = 5u64;
    let content_size = 5u64;
    let storage_size = 5u64;
    let link_count = 5u64;
    let creation_time = 5u64;
    let modification_time = 5u64;
    let domain_value = fidl_conformance::NodeAttributes {
        mode: mode.clone(),
        id: id.clone(),
        content_size: content_size.clone(),
        storage_size: storage_size.clone(),
        link_count: link_count.clone(),
        creation_time: creation_time.clone(),
        modification_time: modification_time.clone(),
    };
    let dynfidl_structure = Structure::default()
        .field(Field::UInt32(mode))
        .field(Field::UInt64(id))
        .field(Field::UInt64(content_size))
        .field(Field::UInt64(storage_size))
        .field(Field::UInt64(link_count))
        .field(Field::UInt64(creation_time))
        .field(Field::UInt64(modification_time));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt16Int32() {
    let a = 7i16;
    let b = 8i32;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt16Int32 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(a)).field(Field::Int32(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt16Int64() {
    let a = 7i16;
    let b = 9i64;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt16Int64 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(a)).field(Field::Int64(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt32Int64() {
    let a = 8i32;
    let b = 9i64;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt32Int64 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(a)).field(Field::Int64(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt8Int16() {
    let a = 6i8;
    let b = 7i16;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt8Int16 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int8(a)).field(Field::Int16(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt8Int32() {
    let a = 6i8;
    let b = 8i32;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt8Int32 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int8(a)).field(Field::Int32(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_PaddingBetweenFieldsInt8Int64() {
    let a = 6i8;
    let b = 9i64;
    let domain_value =
        fidl_conformance::PaddingBetweenFieldsInt8Int64 { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int8(a)).field(Field::Int64(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Regression1() {
    let f1 = 2u8;
    let f2 = 4u32;
    let f3 = 2u8;
    let f4 = 3u16;
    let f5 = 5u64;
    let f6 = 2u8;
    let domain_value = fidl_conformance::Regression1 {
        f1: f1.clone(),
        f2: f2.clone(),
        f3: f3.clone(),
        f4: f4.clone(),
        f5: f5.clone(),
        f6: f6.clone(),
    };
    let dynfidl_structure = Structure::default()
        .field(Field::UInt8(f1))
        .field(Field::UInt32(f2))
        .field(Field::UInt8(f3))
        .field(Field::UInt16(f4))
        .field(Field::UInt64(f5))
        .field(Field::UInt8(f6));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Size5Alignment4() {
    let four = 4u32;
    let one = 2u8;
    let domain_value = fidl_conformance::Size5Alignment4 { four: four.clone(), one: one.clone() };
    let dynfidl_structure =
        Structure::default().field(Field::UInt32(four)).field(Field::UInt8(one));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Size8Align8() {
    let data = 5u64;
    let domain_value = fidl_conformance::Size8Align8 { data: data.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt64(data));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct1Byte() {
    let a = 6i8;
    let domain_value = fidl_conformance::Struct1Byte { a: a.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int8(a));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct2Byte() {
    let a = 7i16;
    let domain_value = fidl_conformance::Struct2Byte { a: a.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(a));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct3Byte() {
    let a = 7i16;
    let b = 6i8;
    let domain_value = fidl_conformance::Struct3Byte { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int16(a)).field(Field::Int8(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct4Byte() {
    let a = 8i32;
    let domain_value = fidl_conformance::Struct4Byte { a: a.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(a));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct5Byte() {
    let a = 8i32;
    let b = 6i8;
    let domain_value = fidl_conformance::Struct5Byte { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(a)).field(Field::Int8(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct6Byte() {
    let a = 8i32;
    let b = 7i16;
    let domain_value = fidl_conformance::Struct6Byte { a: a.clone(), b: b.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(a)).field(Field::Int16(b));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct7Byte() {
    let a = 8i32;
    let b = 7i16;
    let c = 6i8;
    let domain_value = fidl_conformance::Struct7Byte { a: a.clone(), b: b.clone(), c: c.clone() };
    let dynfidl_structure =
        Structure::default().field(Field::Int32(a)).field(Field::Int16(b)).field(Field::Int8(c));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Struct8Byte() {
    let a = 9i64;
    let domain_value = fidl_conformance::Struct8Byte { a: a.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int64(a));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_StructSize16Align8() {
    let f1 = 5u64;
    let f2 = 5u64;
    let domain_value = fidl_conformance::StructSize16Align8 { f1: f1.clone(), f2: f2.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt64(f1)).field(Field::UInt64(f2));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_StructSize3Align2() {
    let f1 = 3u16;
    let f2 = 2u8;
    let domain_value = fidl_conformance::StructSize3Align2 { f1: f1.clone(), f2: f2.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt16(f1)).field(Field::UInt8(f2));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_StructWithInt() {
    let x = 8i32;
    let domain_value = fidl_conformance::StructWithInt { x: x.clone() };
    let dynfidl_structure = Structure::default().field(Field::Int32(x));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_ThreeByte() {
    let elem1 = 2u8;
    let elem2 = 2u8;
    let elem3 = 2u8;
    let domain_value = fidl_conformance::ThreeByte {
        elem1: elem1.clone(),
        elem2: elem2.clone(),
        elem3: elem3.clone(),
    };
    let dynfidl_structure = Structure::default()
        .field(Field::UInt8(elem1))
        .field(Field::UInt8(elem2))
        .field(Field::UInt8(elem3));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_TransformerEmptyStruct() {
    let domain_value = fidl_conformance::TransformerEmptyStruct {};
    let dynfidl_structure = Structure::default();
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint16Struct() {
    let val = 3u16;
    let domain_value = fidl_conformance::Uint16Struct { val: val.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt16(val));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint32Struct() {
    let val = 4u32;
    let domain_value = fidl_conformance::Uint32Struct { val: val.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt32(val));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint64Struct() {
    let val = 5u64;
    let domain_value = fidl_conformance::Uint64Struct { val: val.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt64(val));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint64Uint32Uint16Uint8() {
    let f1 = 5u64;
    let f2 = 4u32;
    let f3 = 3u16;
    let f4 = 2u8;
    let domain_value = fidl_conformance::Uint64Uint32Uint16Uint8 {
        f1: f1.clone(),
        f2: f2.clone(),
        f3: f3.clone(),
        f4: f4.clone(),
    };
    let dynfidl_structure = Structure::default()
        .field(Field::UInt64(f1))
        .field(Field::UInt32(f2))
        .field(Field::UInt16(f3))
        .field(Field::UInt8(f4));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint8Struct() {
    let val = 2u8;
    let domain_value = fidl_conformance::Uint8Struct { val: val.clone() };
    let dynfidl_structure = Structure::default().field(Field::UInt8(val));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
#[test]
fn roundtrip_persistent_conformance_Uint8Uint16Uint32Uint64() {
    let f1 = 2u8;
    let f2 = 3u16;
    let f3 = 4u32;
    let f4 = 5u64;
    let domain_value = fidl_conformance::Uint8Uint16Uint32Uint64 {
        f1: f1.clone(),
        f2: f2.clone(),
        f3: f3.clone(),
        f4: f4.clone(),
    };
    let dynfidl_structure = Structure::default()
        .field(Field::UInt8(f1))
        .field(Field::UInt16(f2))
        .field(Field::UInt32(f3))
        .field(Field::UInt64(f4));
    test_persistent_roundtrip(domain_value, dynfidl_structure);
}
