/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pool_allocator.hpp"

namespace lunatic {

PoolAllocator<u16, 4096, 256> g_pool_alloc;

}
