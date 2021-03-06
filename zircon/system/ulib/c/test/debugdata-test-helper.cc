// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <lib/zx/vmo.h>
#include <zircon/assert.h>
#include <zircon/sanitizer.h>

#include "debugdata.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    return 1;
  }

  if (!strcmp(argv[1], "publish_data")) {
    zx::vmo vmo;
    ZX_ASSERT(zx::vmo::create(zx_system_get_page_size(), 0, &vmo) == ZX_OK);
    ZX_ASSERT(vmo.write(kTestData, 0, sizeof(kTestData)) == ZX_OK);
    ZX_ASSERT(vmo.set_property(ZX_PROP_NAME, kTestName, sizeof(kTestName)) == ZX_OK);
    __sanitizer_publish_data(kTestName, vmo.release());
  } else {
    return 1;
  }

  return 0;
}
