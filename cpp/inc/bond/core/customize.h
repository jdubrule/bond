// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "traits.h"

namespace bond
{


template <typename T> struct 
is_protocol_enabled 
    : false_type {};


struct protocols;

#if !defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
template<typename... Protocol> struct ProtocolList;
#endif

// User can modify set of protocols by specializing customize<protocols>
template <typename> struct
customize
{
    template <typename T> struct
    modify
    {
        typedef T type;
    };
};


}
