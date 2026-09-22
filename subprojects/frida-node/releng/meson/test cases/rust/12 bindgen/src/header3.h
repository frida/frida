// SPDX-license-identifer: Apache-2.0
// Copyright © 2023 Red Hat, Inc

#pragma once

#include "other.h"

int32_t add(const int32_t, const int32_t);

static inline int32_t sub(const int32_t a, const int32_t b) {
   return a - b;
}
