// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(https://fxbug.dev/84961): Fix null safety and remove this language version.
// @dart=2.9

import 'package:inspect_dart_codelab/reverser.dart';
import 'package:fuchsia_logger/logger.dart';
import 'package:test/test.dart';
// CODELAB: Include the inspect test module.

void main() {
  setupLogger(
    name: 'inspect_dart_codelab',
    globalTags: ['part_3', 'unit_test'],
  );

  ReverserImpl openReverser() {
    // [START open_reverser]
    return ReverserImpl(ReverserStats.noop());
    // [END open_reverser]
  }

  test('reverser', () async {
    final reverser = openReverser();
    final result = await reverser.reverse('hello');
    expect(result, equals('olleh'));
    // CODELAB: assert that the inspect data is correct.
  });
}
