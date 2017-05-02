// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

namespace bond
{


namespace detail
{


// default for maybe<T> is 'nothing'
template <typename T>
inline
bool is_default(const maybe<T>& value, const Metadata& /*metadata*/)
{
    return value.is_nothing();
}


// compare basic fields with default value from metadata
template <typename T>
inline
typename boost::enable_if_c<is_basic_type<T>::value 
                        && !is_string_type<T>::value
                        && !is_type_alias<T>::value, bool>::type
is_default(const T& value, const Metadata& metadata)
{
    return (metadata.default_value == value);
}


// compare wire value of type alias fields with default value from metadata
template <typename T>
inline
typename boost::enable_if<is_type_alias<T>, bool>::type 
is_default(const T& value, const Metadata& metadata)
{
    return (metadata.default_value == get_aliased_value(value));
}


// compare string fields with default value from metadata
template <typename T>
inline
typename boost::enable_if<is_string<typename remove_const<T>::type>, bool>::type
is_default(const T& value, const Metadata& metadata)
{
    BOOST_ASSERT(!metadata.default_value.nothing);
    return !metadata.default_value.string_value.compare(0, std::string::npos, string_data(value), string_length(value));
}


template <typename T>
inline
typename boost::enable_if<is_wstring<typename remove_const<T>::type>, bool>::type
is_default(const T& value, const Metadata& metadata)
{
    BOOST_ASSERT(!metadata.default_value.nothing);
    return !metadata.default_value.wstring_value.compare(0, std::wstring::npos, string_data(value), string_length(value));
}


// for containers default value is always empty
template <typename T>
inline
typename boost::enable_if<is_container<T>, bool>::type
is_default(const T& value, const Metadata& /*metadata*/)
{
    return (container_size(value) == 0);
} 


// structs don't have default value
template <typename T>
inline
typename boost::enable_if<is_bond_type<T>, bool>::type
is_default(const T& /*value*/, const Metadata& /*metadata*/)
{
    return false;
} 


// return true if the field may be omitted during serialization
template <typename Writer, typename T>
inline
typename boost::enable_if<may_omit_fields<Writer>, bool>::type
omit_field(const Metadata& metadata, const T& value)
{
    // Validate that all compilation units in a program use the same 
    // specialization of may_omit_fields<Writer>
    (void)one_definition<may_omit_fields<Writer>, true_type>::value;

    // omit the field if it's optional and has default value
    return metadata.modifier == Optional
        && is_default(value, metadata);
}


template <typename Writer, typename T>
inline
typename boost::disable_if<may_omit_fields<Writer>, bool>::type
omit_field(const Metadata& /*metadata*/, const T& /*value*/)
{
    // Validate that all compilation units in a program use the same 
    // specialization of may_omit_fields<Writer>
    (void)one_definition<may_omit_fields<Writer>, false_type>::value;

    // protocol doesn't allow omitting fields
    return false;
}


// when transcoding from one protocol to another fields are never omitted
template <typename Writer, typename Reader, typename T>
inline
bool omit_field(const Metadata& /*metadata*/, const value<T, Reader>& /*value*/)
{
    return false;
}


template <typename T, typename Enable = void> struct
implements_field_omitting 
    : false_type {};


// WriteFieldOmitted is an optional protocol writer method which is called for 
// omitted optional fields. It CAN be implemented by tagged protocols and MUST
// be implemented by untagged protocols that allow omitting optional fields.
template <typename Writer> struct
implements_field_omitting<Writer, 
    typename boost::enable_if<bond::check_method<void (Writer::*)(BondDataType, uint16_t, const Metadata&), &Writer::WriteFieldOmitted> >::type>
    : true_type {};


// ReadFieldOmitted is an optional protocol reader method which MUST be implemented 
// by untagged protocols that allow omitting optional fields.
template <typename Input> struct
implements_field_omitting<Input, 
    typename boost::enable_if<bond::check_method<bool (Input::*)(), &Input::ReadFieldOmitted> >::type>
    : true_type {};


// WriteFieldOmitted
template <typename Writer>
typename boost::enable_if<implements_field_omitting<Writer> >::type
WriteFieldOmitted(Writer& output, BondDataType type, uint16_t id, const Metadata& metadata)
{
    output.WriteFieldOmitted(type, id, metadata);
}


template <typename Writer>
typename boost::disable_if<implements_field_omitting<Writer> >::type
WriteFieldOmitted(Writer& /*output*/, BondDataType /*type*/, uint16_t /*id*/, const Metadata& /*metadata*/)
{}


// ReadFieldOmitted
template <typename Input>
typename boost::enable_if<implements_field_omitting<Input>, bool>::type
ReadFieldOmitted(Input& input)
{
    return input.ReadFieldOmitted();
}


template <typename Input>
typename boost::disable_if<implements_field_omitting<Input>, bool>::type
ReadFieldOmitted(Input& /*input*/)
{
    return false;
}


// ReadStructBegin and ReadStructEnd are optional methods for protocol 
template <typename T, typename Enable = void> struct
implements_struct_begin 
    : false_type {};


// Intially ReadStructBegin/End methods had no parameters but later were extended
// to take a bool parameter indicating deserialization of base part of a struct.
template <typename T, typename Enable = void> struct
implements_struct_begin_with_base
    : false_type {};


template <typename Input> struct
implements_struct_begin<Input, 
    typename boost::enable_if<bond::check_method<void (Input::*)(), &Input::ReadStructBegin> >::type>
    : true_type {};


template <typename Input> struct
implements_struct_begin_with_base<Input, 
    typename boost::enable_if<bond::check_method<void (Input::*)(bool), &Input::ReadStructBegin> >::type>
    : true_type {};


// StructBegin
template <typename Input>
typename boost::enable_if<implements_struct_begin<Input> >::type
StructBegin(Input& input, bool /*base*/)
{
    return input.ReadStructBegin();
}


template <typename Input>
typename boost::enable_if<implements_struct_begin_with_base<Input> >::type
StructBegin(Input& input, bool base)
{
    return input.ReadStructBegin(base);
}


template <typename Input>
typename boost::disable_if_c<implements_struct_begin<Input>::value
                          || implements_struct_begin_with_base<Input>::value>::type
StructBegin(Input& /*input*/, bool /*base*/)
{}


// StructEnd
template <typename Input>
typename boost::enable_if<implements_struct_begin<Input> >::type
StructEnd(Input& input, bool /*base*/)
{
    return input.ReadStructEnd();
}


template <typename Input>
typename boost::enable_if<implements_struct_begin_with_base<Input> >::type
StructEnd(Input& input, bool base)
{
    return input.ReadStructEnd(base);
}


template <typename Input>
typename boost::disable_if_c<implements_struct_begin<Input>::value
                          || implements_struct_begin_with_base<Input>::value>::type
StructEnd(Input& /*input*/, bool /*base*/)
{}


} // namespace detail


// It is OK to omit optional fields with default values for all tagged protocols 
// and for untagged protocols which implement field omitting support.
template <typename T> struct 
may_omit_fields
    : std::integral_constant<bool,
        !uses_static_parser<typename T::Reader>::value
        || detail::implements_field_omitting<T>::value> {};

} // namespace bond
