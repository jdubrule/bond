// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "simple_binary.h"

namespace bond
{

#if defined(BOND_SIMPLE_BINARY_PROTOCOL)
template <typename BufferT>
template <typename T>
inline void SimpleBinaryReader<BufferT>::Skip(const bonded<T, SimpleBinaryReader&>& bonded)
{
    // Skip the structure field-by-field by applying Null transform
    Apply(Null(), bonded);
}


template <typename BufferT>
inline void SimpleBinaryWriter<BufferT>::WriteFieldOmitted(BondDataType type, uint16_t /*id*/, const Metadata& metadata)
{
    // Simple doesn't support omitting fields so instead we write a default value
    BOOST_ASSERT(!metadata.default_value.nothing);

    switch (type)
    {
        case BondDataType::BT_BOOL:
            Write(!!metadata.default_value.uint_value);
            break;
        case BondDataType::BT_UINT8:
            Write(static_cast<uint8_t>(metadata.default_value.uint_value));
            break;
        case BondDataType::BT_UINT16:
            Write(static_cast<uint16_t>(metadata.default_value.uint_value));
            break;
        case BondDataType::BT_UINT32:
            Write(static_cast<uint32_t>(metadata.default_value.uint_value));
            break;
        case BondDataType::BT_UINT64:
            Write(static_cast<uint64_t>(metadata.default_value.uint_value));
            break;
        case BondDataType::BT_FLOAT:
            Write(static_cast<float>(metadata.default_value.double_value));
            break;
        case BondDataType::BT_DOUBLE:
            Write(metadata.default_value.double_value);
            break;
        case BondDataType::BT_STRING:
            Write(metadata.default_value.string_value);
            break;
        case BondDataType::BT_STRUCT:
            BOOST_ASSERT(false);
            break;
        case BondDataType::BT_LIST:
        case BondDataType::BT_SET:
        case BondDataType::BT_MAP:
            WriteContainerBegin(0, type);
            break;
        case BondDataType::BT_INT8:
            Write(static_cast<int8_t>(metadata.default_value.int_value));
            break;
        case BondDataType::BT_INT16:
            Write(static_cast<int16_t>(metadata.default_value.int_value));
            break;
        case BondDataType::BT_INT32:
            Write(static_cast<int32_t>(metadata.default_value.int_value));
            break;
        case BondDataType::BT_INT64:
            Write(static_cast<int64_t>(metadata.default_value.int_value));
            break;
        case BondDataType::BT_WSTRING:
            Write(metadata.default_value.wstring_value);
            break;
        default:
            BOOST_ASSERT(false);
            break;
    }
}
#endif
}
