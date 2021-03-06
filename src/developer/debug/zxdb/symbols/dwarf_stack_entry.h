// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVELOPER_DEBUG_ZXDB_SYMBOLS_DWARF_STACK_ENTRY_H_
#define SRC_DEVELOPER_DEBUG_ZXDB_SYMBOLS_DWARF_STACK_ENTRY_H_

#include <iosfwd>

#include "src/developer/debug/zxdb/common/int128_t.h"
#include "src/developer/debug/zxdb/symbols/base_type.h"

namespace zxdb {

// Represents an entry in the stack for evaluating DWARF 5 expressions.
//
// DWARF 5 introduced "typed" stack entries. Previously, all values were of a generic type. This
// means that every entry has a value plus an optional type which is a reference to a "base" type
//
//   "Each element of the stack has a type and a value, and can represent a value of any supported
//   base type of the target machine. Instead of a base type, elements can have a generic type,
//   which is an integral type that has the size of an address on the target machine and unspecified
//   signedness."
//
// We treat these different values as either signed, unsigned, float, or double. The generic type
// and bools are stored as unsigned.
class DwarfStackEntry {
 public:
  // The DWARF spec says the stack entry "can represent a value of any supported base type of the
  // target machine". We need to support x87 long doubles (80 bits) and XMM registers (128 bits).
  // Generally the XMM registers used for floating point use only the low 64 bits and long doubles
  // are very uncommon, but using 128 bits here covers the edge cases better. The ARM "v" registers
  // (128 bits) are similar.
  //
  // The YMM (256 bit) and ZMM (512 bit) x64 reigisters aren't currently representable in DWARF
  // expressions so larger numbers are unnecessary.
  using SignedType = int128_t;
  using UnsignedType = uint128_t;

  explicit DwarfStackEntry(UnsignedType generic_value);

  // This doesn't do any validation of the data, it just copies data_bytes (up to the maximum size
  // this class supports) and hopes it's the correct type. This is used for deserializing from
  // DWARF where the data is coming in as raw bytes.
  DwarfStackEntry(fxl::RefPtr<BaseType> type, const void* data, size_t data_size);

  // The sign of the BaseType in the first argument must match the sign of the second argument.
  DwarfStackEntry(fxl::RefPtr<BaseType> type, SignedType value);
  DwarfStackEntry(fxl::RefPtr<BaseType> type, UnsignedType value);  // type can be null for generic.
  DwarfStackEntry(fxl::RefPtr<BaseType> type, float value);
  DwarfStackEntry(fxl::RefPtr<BaseType> type, double value);

  // Comparison for unit testing. If types are present, the base type enum and byte size are
  // compared, but not the name nor the identity of the type record.
  bool operator==(const DwarfStackEntry& other) const;
  bool operator!=(const DwarfStackEntry& other) const { return !operator==(other); }

  bool is_generic() const { return !type_; }
  const BaseType* type() const { return type_.get(); }      // Possibly null.
  fxl::RefPtr<BaseType> type_ref() const { return type_; }  // Possibly null.

  // Returns the size in bytes of this value.
  size_t GetByteSize() const;

  UnsignedType unsigned_value() const { return data_.unsigned_value; }
  SignedType signed_value() const { return data_.signed_value; }
  float float_value() const { return data_.float_value; }
  double double_value() const { return data_.double_value; }

  // Some operations need to work on the contained data as an abstract bag of bits. These accessors
  // provide access to it.
  const void* data() const { return &data_; }
  constexpr size_t MaxByteSize() { return sizeof(data_); }

  // These functions also accept null BaseType pointers which are counted as generic.
  static bool TreatAsSigned(const BaseType* type);
  static bool TreatAsUnsigned(const BaseType* type);
  static bool TreatAsFloat(const BaseType* type);
  static bool TreatAsDouble(const BaseType* type);

  bool TreatAsSigned() const { return TreatAsSigned(type_.get()); }
  bool TreatAsUnsigned() const { return TreatAsUnsigned(type_.get()); }
  bool TreatAsFloat() const { return TreatAsFloat(type_.get()); }
  bool TreatAsDouble() const { return TreatAsDouble(type_.get()); }

  bool is_integral() const { return TreatAsSigned() || TreatAsUnsigned(); }

  // Returns true if the value is within the machine epsilon of 0 for the current type.
  bool IsZero() const;

  // Returns true if the two stack entries have the same type, either the same base type, or they
  // are both generic. Most arithmetic operations require them to be the same.
  bool SameTypeAs(const DwarfStackEntry& other) const;

  std::string GetTypeDescription() const;

 private:
  // Null indicates "generic".
  fxl::RefPtr<BaseType> type_;

  // When a type is given, only the low X bytes are relevant (where X is the byte size of the given
  // type). However, the value should be a valid integer (the unused bits will be 0 in the unsigned
  // case, and sign-extended in the signed case).
  //
  // Generic values are treated as unsigned.
  //
  // We do not currently support non-integral stack entries. These are not currently generated by
  // the compiler.
  //
  // NOTE: Some users expect this to be a union! If you know the byte size of the result, some
  // users extract the output as an unsigned and memcpy it to the result to avoid type-checking.
  union {
    UnsignedType unsigned_value;  // Address, boolean, unsigned, unsigned char, UTF.
    SignedType signed_value;      // Signed, signed char.
    float float_value;            // Float, 32-bit.
    double double_value;          // Float, 64-bit.
  } data_;
};

// For test output.
std::ostream& operator<<(std::ostream& out, const DwarfStackEntry& entry);

}  // namespace zxdb

#endif  // SRC_DEVELOPER_DEBUG_ZXDB_SYMBOLS_DWARF_STACK_ENTRY_H_
