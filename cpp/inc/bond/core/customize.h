// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "traits.h"


#if !defined(BOND_COMPACT_BINARY_PROTOCOL) \
 && !defined(BOND_SIMPLE_BINARY_PROTOCOL) \
 && !defined(BOND_FAST_BINARY_PROTOCOL) \
 && !defined(BOND_SIMPLE_JSON_PROTOCOL)

#   define BOND_COMPACT_BINARY_PROTOCOL
#   define BOND_SIMPLE_BINARY_PROTOCOL
#   define BOND_FAST_BINARY_PROTOCOL
#   define BOND_SIMPLE_JSON_PROTOCOL

#endif

namespace bond
{


template <typename T> struct 
is_protocol_enabled 
    : false_type {};


}
