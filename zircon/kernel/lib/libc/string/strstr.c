// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2008 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <string.h>

char *strstr(char const *s1, char const *s2) {
  size_t l1, l2;

  l2 = strlen(s2);
  if (!l2)
    return (char *)s1;
  l1 = strlen(s1);
  while (l1 >= l2) {
    l1--;
    if (!memcmp(s1, s2, l2))
      return (char *)s1;
    s1++;
  }
  return NULL;
}
