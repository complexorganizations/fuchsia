// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package fidl

import (
	"fmt"
	"gen/config"
	"gen/fidl/util"
)

func init() {
	util.Register(config.FidlFile{
		Filename: "float_array.gen.test.fidl",
		Gen:      fidlGenFloatArray,
		Definitions: []config.Definition{
			{
				Config: config.Config{
					"size": 256,
				},
				Denylist: []config.Binding{config.Rust},
			},
		},
	})
}

func fidlGenFloatArray(config config.Config) (string, error) {
	size := config.GetInt("size")
	return fmt.Sprintf(`
struct FloatArray%[1]d {
	array<float32>:%[1]d values;
};`, size), nil
}
