// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Package update contains the `pm update` command
package update

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"go.fuchsia.dev/fuchsia/src/sys/pkg/bin/pm/build"
)

const usage = `Usage: %s update
update the merkle roots in meta/contents
`

// Run executes the `pm update` command
func Run(cfg *build.Config, args []string) error {
	fs := flag.NewFlagSet("update", flag.ExitOnError)

	fs.Usage = func() {
		fmt.Fprintf(os.Stderr, usage, filepath.Base(os.Args[0]))
		fmt.Fprintln(os.Stderr)
		fs.PrintDefaults()
	}

	if err := fs.Parse(args); err != nil {
		return err
	}

	if len(fs.Args()) != 0 {
		fmt.Fprintf(os.Stderr, "WARNING: unused arguments: %s\n", fs.Args())
	}

	return build.Update(cfg)
}
