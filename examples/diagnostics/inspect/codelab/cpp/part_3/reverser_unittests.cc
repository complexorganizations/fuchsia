// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding_set.h>
#include <lib/inspect/cpp/inspect.h>

#include "src/lib/testing/loop_fixture/real_loop_fixture.h"
// CODELAB: Include the inspect test library.

#include "reverser.h"

class ReverserTest : public gtest::RealLoopFixture {
 protected:
  // Creates a Reverser and return a client Ptr for it.
  fuchsia::examples::inspect::ReverserPtr OpenReverser() {
    fuchsia::examples::inspect::ReverserPtr ptr;

    // [START open_reverser]
    binding_set_.AddBinding(std::make_unique<Reverser>(ReverserStats::CreateDefault()),
                            ptr.NewRequest());
    // [END open_reverser]

    return ptr;
  }

  // Get the number of active connections.
  //
  // This allows us to wait until a connection closes.
  size_t connection_count() const { return binding_set_.size(); }

 private:
  fidl::BindingSet<fuchsia::examples::inspect::Reverser, std::unique_ptr<Reverser>> binding_set_;
};

TEST_F(ReverserTest, ReversePart3) {
  auto ptr = OpenReverser();

  bool done = false;
  std::string value;
  ptr->Reverse("hello", [&](std::string response) {
    value = std::move(response);
    done = true;
  });
  RunLoopUntil([&] { return done; });
  EXPECT_EQ("olleh", value);
}
