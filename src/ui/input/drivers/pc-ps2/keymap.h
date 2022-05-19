// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_UI_INPUT_DRIVERS_PC_PS2_KEYMAP_H_
#define SRC_UI_INPUT_DRIVERS_PC_PS2_KEYMAP_H_

#include <fidl/fuchsia.input/cpp/wire.h>
#include <stdint.h>

namespace i8042 {

constexpr uint8_t kKeyUp = 0x80;
constexpr uint8_t kScancodeMask = 0x7f;

constexpr uint8_t kExtendedScancode = 0xe0;

inline constexpr std::optional<fuchsia_input::wire::Key> kSet1UsageMap[128] = {
    /* 0x00 */ std::nullopt,
    fuchsia_input::wire::Key::kEscape,
    fuchsia_input::wire::Key::kKey1,
    fuchsia_input::wire::Key::kKey2,
    /* 0x04 */ fuchsia_input::wire::Key::kKey3,
    fuchsia_input::wire::Key::kKey4,
    fuchsia_input::wire::Key::kKey5,
    fuchsia_input::wire::Key::kKey6,
    /* 0x08 */ fuchsia_input::wire::Key::kKey7,
    fuchsia_input::wire::Key::kKey8,
    fuchsia_input::wire::Key::kKey9,
    fuchsia_input::wire::Key::kKey0,
    /* 0x0c */ fuchsia_input::wire::Key::kMinus,
    fuchsia_input::wire::Key::kEquals,
    fuchsia_input::wire::Key::kBackspace,
    fuchsia_input::wire::Key::kTab,
    /* 0x10 */ fuchsia_input::wire::Key::kQ,
    fuchsia_input::wire::Key::kW,
    fuchsia_input::wire::Key::kE,
    fuchsia_input::wire::Key::kR,
    /* 0x14 */ fuchsia_input::wire::Key::kT,
    fuchsia_input::wire::Key::kY,
    fuchsia_input::wire::Key::kU,
    fuchsia_input::wire::Key::kI,
    /* 0x18 */ fuchsia_input::wire::Key::kO,
    fuchsia_input::wire::Key::kP,
    fuchsia_input::wire::Key::kLeftBrace,
    fuchsia_input::wire::Key::kRightBrace,
    /* 0x1c */ fuchsia_input::wire::Key::kEnter,
    fuchsia_input::wire::Key::kLeftCtrl,
    fuchsia_input::wire::Key::kA,
    fuchsia_input::wire::Key::kS,
    /* 0x20 */ fuchsia_input::wire::Key::kD,
    fuchsia_input::wire::Key::kF,
    fuchsia_input::wire::Key::kG,
    fuchsia_input::wire::Key::kH,
    /* 0x24 */ fuchsia_input::wire::Key::kJ,
    fuchsia_input::wire::Key::kK,
    fuchsia_input::wire::Key::kL,
    fuchsia_input::wire::Key::kSemicolon,
    /* 0x28 */ fuchsia_input::wire::Key::kApostrophe,
    fuchsia_input::wire::Key::kGraveAccent,
    fuchsia_input::wire::Key::kLeftShift,
    fuchsia_input::wire::Key::kBackslash,
    /* 0x2c */ fuchsia_input::wire::Key::kZ,
    fuchsia_input::wire::Key::kX,
    fuchsia_input::wire::Key::kC,
    fuchsia_input::wire::Key::kV,
    /* 0x30 */ fuchsia_input::wire::Key::kB,
    fuchsia_input::wire::Key::kN,
    fuchsia_input::wire::Key::kM,
    fuchsia_input::wire::Key::kComma,
    /* 0x34 */ fuchsia_input::wire::Key::kDot,
    fuchsia_input::wire::Key::kSlash,
    fuchsia_input::wire::Key::kRightShift,
    fuchsia_input::wire::Key::kKeypadAsterisk,
    /* 0x38 */ fuchsia_input::wire::Key::kLeftAlt,
    fuchsia_input::wire::Key::kSpace,
    fuchsia_input::wire::Key::kCapsLock,
    fuchsia_input::wire::Key::kF1,
    /* 0x3c */ fuchsia_input::wire::Key::kF2,
    fuchsia_input::wire::Key::kF3,
    fuchsia_input::wire::Key::kF4,
    fuchsia_input::wire::Key::kF5,
    /* 0x40 */ fuchsia_input::wire::Key::kF6,
    fuchsia_input::wire::Key::kF7,
    fuchsia_input::wire::Key::kF8,
    fuchsia_input::wire::Key::kF9,
    /* 0x44 */ fuchsia_input::wire::Key::kF10,
    fuchsia_input::wire::Key::kNumLock,
    fuchsia_input::wire::Key::kScrollLock,
    fuchsia_input::wire::Key::kKeypad7,
    /* 0x48 */ fuchsia_input::wire::Key::kKeypad8,
    fuchsia_input::wire::Key::kKeypad9,
    fuchsia_input::wire::Key::kKeypadMinus,
    fuchsia_input::wire::Key::kKeypad4,
    /* 0x4c */ fuchsia_input::wire::Key::kKeypad5,
    fuchsia_input::wire::Key::kKeypad6,
    fuchsia_input::wire::Key::kKeypadPlus,
    fuchsia_input::wire::Key::kKeypad1,
    /* 0x50 */ fuchsia_input::wire::Key::kKeypad2,
    fuchsia_input::wire::Key::kKeypad3,
    fuchsia_input::wire::Key::kKeypad0,
    fuchsia_input::wire::Key::kKeypadDot,
    /* 0x54 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kF11,
    /* 0x58 */ fuchsia_input::wire::Key::kF12,
    std::nullopt,
    std::nullopt,
    std::nullopt,
};

inline constexpr std::optional<fuchsia_input::wire::Key> kSet1ExtendedUsageMap[128] = {
    /* 0x00 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x08 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x10 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x18 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kKeypadEnter,
    fuchsia_input::wire::Key::kRightCtrl,
    std::nullopt,
    std::nullopt,
    /* 0x20 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x28 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kMediaVolumeDecrement,
    std::nullopt,
    /* 0x30 */ fuchsia_input::wire::Key::kMediaVolumeIncrement,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kKeypadSlash,
    std::nullopt,
    fuchsia_input::wire::Key::kPrintScreen,
    /* 0x38 */ fuchsia_input::wire::Key::kRightAlt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x40 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kHome,
    /* 0x48 */ fuchsia_input::wire::Key::kUp,
    fuchsia_input::wire::Key::kPageUp,
    std::nullopt,
    fuchsia_input::wire::Key::kLeft,
    std::nullopt,
    fuchsia_input::wire::Key::kRight,
    std::nullopt,
    fuchsia_input::wire::Key::kEnd,
    /* 0x50 */ fuchsia_input::wire::Key::kDown,
    fuchsia_input::wire::Key::kPageDown,
    fuchsia_input::wire::Key::kInsert,
    fuchsia_input::wire::Key::kDelete,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    /* 0x58 */ std::nullopt,
    std::nullopt,
    std::nullopt,
    fuchsia_input::wire::Key::kLeftMeta,
    fuchsia_input::wire::Key::kRightMeta,
    std::nullopt /* MENU */,
    std::nullopt,
    std::nullopt,
};

}  // namespace i8042

#endif  // SRC_UI_INPUT_DRIVERS_PC_PS2_KEYMAP_H_
