// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "exception.h"
#include "protocol.h"
#include "runtime_schema.h"
#include "exception.h"

namespace bond
{
namespace detail
{


// Overload of Apply used to extract bonded<T> from marshaled payload
template <typename T, typename U, typename Reader>
inline bool
Apply(const boost::reference_wrapper<bonded<T> >& ref, const bonded<U, Reader>& value)
{
    value.Deserialize(ref.get());
    return false;
}


// Select protocol and apply transform using compile-time schema
template <typename T, typename Buffer, typename Transform>
inline std::pair<ProtocolType, bool> NextProtocol(
    const boost::mpl::l_iter<boost::mpl::l_end>&,
    Buffer&,
    const Transform&)
{
    UnknownProtocolException();
    return std::make_pair(ProtocolType::MARSHALED_PROTOCOL, false);
}


template <typename T, typename Buffer, typename Transform, typename Iter>
inline std::pair<ProtocolType, bool> NextProtocol(
    const Iter&,
    Buffer& input,
    const Transform& transform)
{
    typedef typename boost::mpl::deref<Iter>::type Reader;

    Reader reader(input);

    if (reader.ReadVersion())
    {
        return std::make_pair(
            static_cast<ProtocolType>(Reader::magic),
            Apply(transform, bonded<T, ProtocolReader<Buffer> >(reader)));
    }
    else
    {
        return NextProtocol<T>(typename boost::mpl::next<Iter>::type(), input, transform);
    }
}

// Select protocol and apply transform using runtime schema
template <typename Buffer, typename Transform>
inline std::pair<ProtocolType, bool> NextProtocol(
    const boost::mpl::l_iter<boost::mpl::l_end>&, 
    const RuntimeSchema&, 
    Buffer&, 
    const Transform&)
{
    UnknownProtocolException(); 
    return std::make_pair(ProtocolType::MARSHALED_PROTOCOL, false);
}


template <typename Buffer, typename Transform, typename Iter>
inline std::pair<ProtocolType, bool> NextProtocol(
    const Iter&, 
    const RuntimeSchema& schema, 
    Buffer& input, 
    const Transform& transform)
{
    typedef typename boost::mpl::deref<Iter>::type Reader;
    
    Reader reader(input);

    if (reader.ReadVersion())
    {
        return std::make_pair(
            static_cast<ProtocolType>(Reader::magic), 
            Apply(transform, bonded<void, ProtocolReader<Buffer> >(reader, schema)));
    }
    else
    {
        return NextProtocol(typename boost::mpl::next<Iter>::type(), schema, input, transform);
    }
}

#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)

// Select protocol based on magic number and apply transform using compile-time schema
template <typename T, typename Buffer, typename Transform>
inline bool NextProtocol(
    const boost::mpl::l_iter<boost::mpl::l_end>&,
    Buffer&, const Transform&, uint16_t protocol)
{
    UnknownProtocolException(protocol);
    return false;
}


template <typename T, typename Buffer, typename Transform, typename Iter>
inline bool NextProtocol(
    const Iter&,
    Buffer& input,
    const Transform& transform,
    uint16_t protocol)
{
    typedef typename boost::mpl::deref<Iter>::type Reader;

    if (Reader::magic == protocol)
    {
        Reader reader(input);
        return Apply(transform, bonded<T, Reader&>(reader));
    }
    else
    {
        return NextProtocol<T>(typename boost::mpl::next<Iter>::type(), input, transform, protocol);
    }
}


// Select protocol based on magic number and apply transform using runtime schema
template <typename Buffer, typename Transform>
inline bool NextProtocol(
    const boost::mpl::l_iter<boost::mpl::l_end>&,
    const RuntimeSchema&, Buffer&, const Transform&, uint16_t protocol)
{
    UnknownProtocolException(protocol);
    return false;
}


template <typename Buffer, typename Transform, typename Iter>
inline bool NextProtocol(
    const Iter&,
    const RuntimeSchema& schema,
    Buffer& input,
    const Transform& transform,
    uint16_t protocol)
{
    typedef typename boost::mpl::deref<Iter>::type Reader;

    if (Reader::magic == protocol)
    {
        Reader reader(input);
        return Apply(transform, bonded<void, Reader&>(reader, schema));
    }
    else
    {
        return NextProtocol(typename boost::mpl::next<Iter>::type(), schema, input, transform, protocol);
    }
}

// Select protocol based on magic number and apply instance of serializing transform 
template <template <typename Writer> class Transform, typename Buffer, typename T>
inline bool NextProtocol(
    const boost::mpl::l_iter<boost::mpl::l_end>&, 
    const T&, Buffer&, uint16_t protocol)
{
    UnknownProtocolException(protocol);
    return false;
}


template <template <typename Writer> class Transform, typename Buffer, typename T, typename Iter>
inline bool NextProtocol(
    const Iter&, 
    const T& value, 
    Buffer& output, 
    uint16_t protocol)
{
    typedef typename boost::mpl::deref<Iter>::type Reader;

    if (Reader::magic == protocol)
    {
        typename Reader::Writer writer(output);
        return Apply(Transform<typename Reader::Writer>(writer), value);
    }
    else
    {
        return NextProtocol<Transform>(typename boost::mpl::next<Iter>::type(), value, output, protocol);
    }
}
#else


template<typename Buffer, typename Protocols = Protocols<Buffer>::type>
struct DoProtocolApply;

template<typename Buffer, typename... P>
struct DoProtocolApply<Buffer, ProtocolList<P...>>
{
    // Call on all protocols, stop when matched.
    template<typename F>
    DoProtocolApply(F applyFunc)
    {
        std::pair<ProtocolType, bool> result{ MARSHALED_PROTOCOL, false };

        auto doThese =
        {
            result.second == MARSHALED_PROTOCOL && ((result = applyFunc(std::common_type<P,P>())), false)...
        };

        if (result.second == MARSHALED_PROTOCOL)
        {
            UnknownProtocolException();
        }
        _result = result;
    }

    // Call on matching protocol.
    template<typename F>
    DoProtocolApply(uint16_t protocol, F applyFunc)
    {
        bool matched = false;
        std::pair<ProtocolType, bool> result = std::make_pair(MARSHALED_PROTOCOL, false);

        auto doThese =
        {
            protocol == P::magic && (matched = true) && ((result = std::make_pair(P::magic, applyFunc(std::common_type<P,P>()))), false)...
        };

        if (!matched)
        {
            UnknownProtocolException(protocol);
        }
        _result = result;
    }

    operator bool() const
    {
        return _result.second;
    }

    operator std::pair<ProtocolType, bool>() const
    {
        return _result;
    }

    std::pair<ProtocolType, bool> _result;
};

#endif // BOND_NO_CXX11_VARIADIC_TEMPLATES

template <typename T, typename Buffer, typename Transform>
inline bool ApplyMatchingProtocol(
    Buffer& input,
    const Transform& transform,
    uint16_t protocol)
{
#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
    return detail::NextProtocol<T>(
        typename Protocols<Buffer>::begin(),
        input,
        transform,
        protocol
        );
#else
    return DoProtocolApply<Buffer>(protocol, [&input, &transform, &result](auto & readerType)
    {
        decltype(readerType)::type reader(input);
        return Apply(transform, bonded<T, Reader&>(reader));
    });
#endif
}

template <typename Buffer, typename Transform>
inline bool ApplyMatchingProtocol(
    const Transform& transform,
    const RuntimeSchema& schema,
    Buffer& input,
    uint16_t protocol)
{
#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
    return detail::NextProtocol(
        typename Protocols<Buffer>::begin(),
        schema,
        input,
        transform,
        protocol
        );
#else
    return DoProtocolApply<Buffer>(protocol, [&input, &transform, &schema, &result](autreaderType)
    {
        decltype(readerType)::type reader(input);
        result = Apply(transform, bonded<void, Reader&>(reader, schema));
    });
#endif
}

template <template <typename Writer> class Transform, typename Buffer, typename T>
inline bool ApplyMatchingProtocol(
    const T& value,
    Buffer& output,
    uint16_t protocol)
{
#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
    return detail::NextProtocol<Transform>(
        typename Protocols<Buffer>::begin(),
        value,
        output,
        protocol);
#else
    return DoProtocolApply<Buffer>(protocol, [&output, &value, &result](auto & readerType)
    {
        decltype(readerType)::type::Writer writer(output);
        result = Apply(Transform<decltype(writer)>(writer), value);
    });
#endif
}

} // namespace detail


//
// Apply transform to serialized data that was generated using Marshaler 
//

// Use compile-time schema
template <typename T, typename Buffer, typename Transform>
inline std::pair<ProtocolType, bool> SelectProtocolAndApply(
    Buffer& input, 
    const Transform& transform)
{
#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES) || 1 // FIXME
    return detail::NextProtocol<T>(typename Protocols<Buffer>::begin(), input, transform);
#else
    return detail::DoProtocolApply<Buffer>([&input, &transform](const auto readerType)
    {
        (readerType);
        decltype(readerType)::type reader(input);

        if (reader.ReadVersion())
        {
            return std::make_pair(
                static_cast<ProtocolType>(decltype(readerType)::type::magic),
                Apply(transform, bonded<T, ProtocolReader<Buffer> >(reader)));
        }
        else
        {
            return std::make_pair(MARSHALED_PROTOCOL, false);
        }
    });
#endif
}


// Use runtime schema
template <typename Buffer, typename Transform>
inline std::pair<ProtocolType, bool> SelectProtocolAndApply(
    const RuntimeSchema& schema,
    Buffer& input, 
    const Transform& transform)
{
#if 1 // defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
    return detail::NextProtocol(typename Protocols<Buffer>::begin(), schema, input, transform);
#else
    return detail::DoProtocolApply<Buffer>([&schema, &input, &transform](const auto readerType)
    {
        (readerType);
        decltype(readerType)::type reader(input);

        if (reader.ReadVersion())
        {
            return std::make_pair(
                static_cast<ProtocolType>(decltype(readerType)::type::magic),
                Apply(transform, bonded<void, ProtocolReader<Buffer> >(reader, schema)));
        }
        else
        {
            return std::make_pair(MARSHALED_PROTOCOL, false);
        }
    });
#endif
}


// Apply deserializing transform with a protocol specified by magic number 
// Use compile-time schema
template <typename T, typename Buffer, typename Transform>
inline bool Apply(
    const Transform& transform, 
    Buffer& input, 
    uint16_t protocol)
{
    return detail::ApplyMatchingProtocol<T>(transform, input, protocol);
}


// Use runtime schema
template <typename Buffer, typename Transform>
inline bool Apply(
    const Transform& transform, 
    const RuntimeSchema& schema, 
    Buffer& input, 
    uint16_t protocol)
{
    return detail::ApplyMatchingProtocol(
        schema,
        input,
        transform,
        protocol);
}


// Apply an instance of serializing transform with a protocol specified by magic number
template <template <typename Writer> class Transform, typename Buffer, typename T>
inline bool Apply(const T& value, Buffer& output, uint16_t protocol)
{
    return detail::ApplyMatchingProtocol<Transform>(value, output, protocol);
}

} // namespace bond

