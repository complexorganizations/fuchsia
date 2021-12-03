// Copyright 2021 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <lib/uart/all.h>
#include <lib/uart/null.h>

#include <dev/hdcp/amlogic_s912/init.h>
#include <dev/hw_rng/amlogic_rng/init.h>
#include <dev/hw_watchdog/generic32/init.h>
#include <dev/psci.h>
#include <dev/timer/arm_generic.h>
#include <dev/uart/amlogic_s905/init.h>
#include <dev/uart/pl011/init.h>
#include <ktl/type_traits.h>
#include <ktl/variant.h>
#include <phys/arch/arch-handoff.h>

namespace {

// Overloads for early UART initialization below.
void UartInitEarly(uint32_t extra, const uart::null::Driver::config_type& config) {}

void UartInitEarly(uint32_t extra, const dcfg_simple_t& config) {
  // TODO(fxbug.dev/89452): Handle all cases below.
  switch (extra) {
    case KDRV_AMLOGIC_UART:
      AmlogicS905UartInitEarly(config);
      break;
    case KDRV_DW8250_UART:
    case KDRV_MOTMOT_UART:
      break;
    case KDRV_PL011_UART:
      Pl011UartInitEarly(config);
      break;
  }
}

void UartInitLate(uint32_t extra) {
  // TODO(fxbug.dev/89452): Handle all cases below.
  switch (extra) {
    case KDRV_AMLOGIC_UART:
      AmlogicS905UartInitLate();
      break;
    case KDRV_DW8250_UART:
    case KDRV_MOTMOT_UART:
      break;
    case KDRV_PL011_UART:
      Pl011UartInitLate();
      break;
  }
}

}  // namespace

void ArchDriverHandoffEarly(const ArchPhysHandoff& arch_handoff) {
  if (arch_handoff.generic_32bit_watchdog_driver) {
    Generic32BitWatchdogEarlyInit(arch_handoff.generic_32bit_watchdog_driver.value());
  }

  if (arch_handoff.generic_timer_driver) {
    ArmGenericTimerInit(arch_handoff.generic_timer_driver.value());
  }

  if (arch_handoff.psci_driver) {
    PsciInit(arch_handoff.psci_driver.value());
  }
}

void ArchDriverHandoffLate(const ArchPhysHandoff& arch_handoff) {
  if (arch_handoff.amlogic_hdcp_driver) {
    AmlogicS912HdcpInit(arch_handoff.amlogic_hdcp_driver.value());
  }

  if (arch_handoff.amlogic_rng_driver) {
    AmlogicRngInit(arch_handoff.amlogic_rng_driver.value());
  }

  if (arch_handoff.generic_32bit_watchdog_driver) {
    Generic32BitWatchdogLateInit();
  }
}

void ArchUartDriverHandoffEarly(const uart::all::Driver& serial) {
  ktl::visit([](const auto& uart) { UartInitEarly(uart.extra(), uart.config()); }, serial);
}

void ArchUartDriverHandoffLate(const uart::all::Driver& serial) {
  ktl::visit([](const auto& uart) { UartInitLate(uart.extra()); }, serial);
}
