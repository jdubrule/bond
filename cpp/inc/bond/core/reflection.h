// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <boost/static_assert.hpp>

#include <bond/core/config.h>
#if defined(BOND_ENABLE_PRECXX11_MPL_SCHEMAS)
#include <boost/mpl/transform.hpp>
#include <boost/mpl/find_if.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/copy_if.hpp>
#endif

#if defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
#include <boost/assign.hpp>
#include <boost/assign/list_of.hpp>
#endif

#include <bond/core/bond_types.h>
#include "bonded.h"
#include "detail/metadata.h"

namespace bond
{

#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
using boost::mpl::_;
#endif

template <typename T> struct 
remove_maybe
{
    typedef T type;
};


template <typename T> struct 
remove_maybe<maybe<T> >
{
    typedef typename remove_maybe<T>::type type;
};


static const uint16_t invalid_field_id = 0xffff;


/** namespace bond::reflection */
namespace reflection
{
//
// Helper classes/templates
//
typedef std::map<std::string, std::string> Attributes;


// field is required
struct required_field_modifier
{
    static const bond::Modifier value = bond::Modifier::Required;
};

// field is optional
struct optional_field_modifier 
{
    static const bond::Modifier value = bond::Modifier::Optional;
};

// field is required optional
struct required_optional_field_modifier
{
    static const bond::Modifier value = bond::Modifier::RequiredOptional;
};


/// @brief Field description in compile-time schema
template 
<
    uint16_t t_id,
    typename t_modifierTag,
    typename t_struct,
    typename t_fieldType,
    t_fieldType t_struct::*t_fieldAddr,
    const bond::Metadata* t_metadata
>
struct FieldTemplate
{
    static constexpr bool is_nested_field() { return is_bond_type<field_type>::value; }

    /// @brief Type of the field's parent struct
    typedef t_struct struct_type;

    /// @brief Type of the field pointer
    typedef t_fieldType t_struct::*field_pointer;
    
    /// @brief Type of the field
    typedef typename remove_maybe<t_fieldType>::type field_type;
    
    /// @brief Type of the field value
    typedef t_fieldType value_type;
    
    /// @brief Modifier tag for the field
    ///
    /// Can be one of:
    /// - bond::reflection::optional_field_modifier
    /// - bond::reflection::required_field_modifier
    /// - bond::reflection::required_optional_field_modifier
    typedef t_modifierTag field_modifier;

    /// @brief Static data member describing field metadata
    static const Metadata& metadata;
    
    /// @brief Static data member representing the field pointer
    static const field_pointer field_ptr;

    /// @brief Static data member equal to the field ordinal
    static const uint16_t id = t_id;

    /// @brief Static method returning const reference to the field value for a particular object
    static 
    const t_fieldType& GetVariable(const struct_type& object)
    {
        return object.*t_fieldAddr;
    }

    /// @brief Static method returning reference to the field value for a particular object
    static 
    t_fieldType& GetVariable(struct_type& object) 
    {
        return object.*t_fieldAddr;
    }

    BOOST_STATIC_ASSERT(t_id != invalid_field_id);
};


template 
<
    uint16_t t_id,
    typename t_modifierTag,
    typename t_struct,
    typename t_fieldType,
    t_fieldType t_struct::*t_fieldAddr,
    const Metadata* t_metadata
>
const bond::Metadata& FieldTemplate<t_id, t_modifierTag, t_struct, t_fieldType, t_fieldAddr, t_metadata>::metadata = *t_metadata;

template 
<
    uint16_t t_id,
    typename t_modifierTag,
    typename t_struct,
    typename t_fieldType,
    t_fieldType t_struct::*t_fieldAddr,
    const Metadata* t_metadata
>
const typename FieldTemplate<t_id, t_modifierTag, t_struct, t_fieldType, t_fieldAddr, t_metadata>::field_pointer
      FieldTemplate<t_id, t_modifierTag, t_struct, t_fieldType, t_fieldAddr, t_metadata>::field_ptr = t_fieldAddr;

// Metadata initializer for fields
inline 
bond::Metadata MetadataInit(const char* name)
{
    bond::Metadata metadata;

    metadata.name = name;
    return metadata;
}

inline 
bond::Metadata MetadataInit(const char* name, bond::Modifier modifier, const Attributes& attributes)
{
    bond::Metadata metadata;

    metadata.name = name;
    metadata.modifier = modifier;
    metadata.attributes = attributes;

    return metadata;
}

template <typename T>
bond::Metadata MetadataInit(const T& default_value, const char* name)
{
    bond::Metadata metadata;

    metadata.name = name;
    detail::VariantSet(metadata.default_value, default_value);
    return metadata;
}

template <typename T>
bond::Metadata MetadataInit(const T& default_value, const char* name, bond::Modifier modifier, const Attributes& attributes)
{
    bond::Metadata metadata = MetadataInit(name, modifier, attributes);

    detail::VariantSet(metadata.default_value, default_value);

    return metadata;
}

struct nothing
{};

inline
bond::Metadata MetadataInit(const nothing&, const char* name)
{
    bond::Metadata metadata;

    metadata.name = name;
    metadata.default_value.nothing = true;
    return metadata;
}

inline
bond::Metadata MetadataInit(const nothing&, const char* name, bond::Modifier modifier, const Attributes& attributes)
{
    bond::Metadata metadata = MetadataInit(name, modifier, attributes);

    metadata.default_value.nothing = true;

    return metadata;
}


// Metadata initializer for structs
inline 
bond::Metadata MetadataInit(const char* name, const char* qualified_name, const Attributes& attributes)
{
    bond::Metadata metadata;

    metadata.name = name;
    metadata.qualified_name = qualified_name;
    metadata.attributes = attributes;

    return metadata;
}


// Metadata initializer for generic structs
#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
template <typename Params>
bond::Metadata MetadataInit(const char* name, const char* qualified_name, const Attributes& attributes)
{
    bond::Metadata metadata = MetadataInit(name, qualified_name, attributes);

    std::string params;

    // boost::mpl::for_each instantiates object of each type in the sequence. 
    // We transform the Params to a sequence of type pointers to avoid creating
    // actual complex types that might not even support default ctor.
    typedef typename boost::mpl::transform<Params, boost::add_pointer<_> >::type ParamsPtr;
    
    boost::mpl::for_each<ParamsPtr>(detail::TypeListBuilder(params));
    
    metadata.name += "<" + params + ">";
    metadata.qualified_name += "<" + params + ">";

    return metadata;
}
#else

template<typename... Params>
bond::Metadata MetadataInit(const char* name, const char* qualified_name, const Attributes& attributes)
{
    bond::Metadata metadata = MetadataInit(name, qualified_name, attributes);

    std::string params;
    detail::TypeListBuilder typeListBuilder(params);

    auto doThese =
    {
        0,
        ((void)typeListBuilder((Params*)nullptr),0)...
    };

    metadata.name += "<" + params + ">";
    metadata.qualified_name += "<" + params + ">";

    return metadata;
}
#endif

} // namespace reflection


const reflection::nothing nothing = {};

#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)

template <typename T, uint16_t minId = 0> struct 
next_required_field
{
private:
    template <typename Field> struct 
    is_next_required
    {
        static const bool value = Field::id >= minId 
                               && is_same<typename Field::field_modifier, typename reflection::required_field_modifier>::value;
    };

public:
    static const uint16_t value = field_id<T, typename boost::mpl::find_if<T, is_next_required<_> >::type>::value;
};

#else

namespace detail
{
    template<typename t_Schema, typename t_currentFieldIndex, uint16_t minId>
    struct next_required_field_helper
    {
    private:
        static const uint16_t currentFieldId = t_Schema::field<t_currentFieldIndex::value>::type::id;
        static const bool currentFieldIsRequired = std::is_same<t_Schema::field<t_currentFieldIndex::value>::type::field_modifier, reflection::required_field_modifier>::value;

	public:
        static const uint16_t value = currentFieldId >= minId && currentFieldIsRequired ?
            currentFieldId
            : next_required_field_helper<t_Schema, std::integral_constant<size_t, t_currentFieldIndex::value + 1>, minId>::value;
    };

    template<typename t_Schema, uint16_t minId>
    struct next_required_field_helper<t_Schema, std::integral_constant<size_t, t_Schema::fieldCount>, minId>:
        std::integral_constant<uint16_t, invalid_field_id>
    {};
}

template<typename TField, uint16_t minId = 0>
struct next_required_field
{
private:
    typedef typename schema<typename TField::struct_type>::type t_Schema;

public:

    static const uint16_t value = detail::next_required_field_helper<t_Schema, t_Schema::fieldIndex<TField>, minId>::value;
};

#endif

template<typename T>
struct is_empty_struct :
    std::integral_constant<bool, schema<T>::type::fieldCount == 0>
{};

template<bool ...Tail>
struct are_all_true : std::true_type {};

template<bool Head, bool ...Tail>
struct are_all_true<Head, Tail...>
{
    static const bool value = Head && are_all_true<Tail...>::value;
};

template<typename t_Schema, size_t t_fieldToStartAt = 0, typename Seq = std::make_index_sequence<t_Schema::fieldCount - t_fieldToStartAt>>
struct any_required_fields;

template<typename t_Schema, size_t t_fieldToStartAt, size_t... S>
struct any_required_fields<t_Schema, t_fieldToStartAt, std::index_sequence<S...>>
{
    static const bool value = !are_all_true<
        !std::is_same<typename t_Schema::field<S + t_fieldToStartAt>::type::field_modifier, reflection::required_field_modifier>::value...
    >::value;
};

struct no_base {};


template <typename T, typename Enable = void> struct 
is_writer 
    : false_type {};


template <typename T> struct 
is_writer<T, typename boost::enable_if<check_method<void (T::*)(const Metadata&, bool), &T::WriteStructBegin> >::type>
     : true_type {};


template <typename T> 
inline typename T::base* 
base_class()
{
    return NULL;
}


template <typename T> struct 
remove_bonded
{
    typedef T type;
};


template <typename T> struct 
remove_bonded<bonded<T> >
{
    typedef typename remove_bonded<T>::type type;
};


template <typename T> struct 
is_bond_type
{
    typedef typename remove_const<T>::type U;

    static const bool value = is_bonded<U>::value
                           || has_schema<U>::value;
};


struct Unknown;

template <typename Unused> struct
schema<Unknown, Unused>
{
    struct type
    {
        typedef no_base base;
#if defined(BOND_ENABLE_PRECXX11_MPL_SCHEMAS)
        typedef boost::mpl::list<>::type fields;
#endif

#if !defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
        static const size_t fieldCount = 0;
#endif

        static const Metadata metadata;

        type()
        {
            (void)metadata;
        }
    };
};

template <typename Unused> 
const Metadata schema<Unknown, Unused>::type::metadata 
    = reflection::MetadataInit("Unknown", "Unknown", reflection::Attributes());

template <typename T, typename Enable> struct 
schema_for_passthrough
    : schema<typename remove_bonded<T>::type>
{};

template <typename T> struct 
schema_for_passthrough<T, typename boost::disable_if<has_schema<typename remove_bonded<T>::type> >::type>
{
    // If type T doesn't have schema we return schema of an empty struct;
    // this allows pass-through of bonded<T> with only forward declaration for T.
    typedef typename schema<Unknown>::type type;
};

template <typename T> struct 
is_container
{
    typedef typename remove_const<T>::type U;

    static const bool value = is_list_container<U>::value
                           || is_set_container<U>::value
                           || is_map_container<U>::value;
};


template <typename Field, typename Transform, typename Enable = void> struct
is_fast_path_field 
    : false_type {};


template <typename Field, typename Transform> struct
is_fast_path_field<Field, Transform, typename boost::enable_if<is_same<typename Field::struct_type, 
                                                                       typename Transform::FastPathType> >::type>
     : true_type {};


template <typename T> struct 
is_nested_field
    : is_bond_type<typename T::field_type> {}; 

template <typename T> struct 
is_struct_field
    : has_schema<typename T::field_type> {}; 


template <typename T1, typename T2, typename Enable = void> struct 
is_matching_container 
    : false_type {};


template <typename T1, typename T2> struct 
is_matching_basic
{
    static const bool value = 
           (is_string<T1>::value && is_string<T2>::value)
        || (is_wstring<T1>::value && is_wstring<T2>::value)
        || ((sizeof(T1) <= sizeof(T2))
         && ((is_unsigned<T1>::value && is_unsigned<T2>::value) 
          || (is_signed_int_or_enum<T1>::value && is_signed_int_or_enum<T2>::value)));
};


template <typename T> struct
is_matching_basic<typename aliased_type<T>::type, T>
    : boost::true_type {};


template <typename T1, typename T2> struct 
is_matching
{
    static const bool value = 
           (is_bond_type<T1>::value && is_bond_type<T2>::value)
        || (is_matching_basic<T1, T2>::value)
        || (is_matching_container<T1, T2>::value);
};


template <typename T> struct 
is_matching_basic<T, T> 
    : true_type {};


template <> struct 
is_matching_basic<bool, bool> 
    : true_type {};


template <typename T> struct 
is_matching_basic<bool, T> 
    : false_type {};


template <> struct 
is_matching_basic<uint8_t, bool> 
    : false_type {};


template <> struct 
is_matching_basic<float, double> 
    : true_type {};


template <typename T> struct 
get_type_id;


template <typename T1, typename T2> struct 
is_matching_container<T1, T2, 
    typename boost::enable_if_c<is_container<T1>::value  
                             && get_type_id<T1>::value == get_type_id<T2>::value>::type>
    : is_matching<typename element_type<T1>::type, 
                  typename element_type<T2>::type> {};


// tuples match if the elements match
template <typename T1, typename T2, typename U1, typename U2> struct 
is_matching<std::pair<T1, T2>, std::pair<U1, U2> >
{
    static const bool value = is_matching<T1, U1>::value 
                           && is_matching<T2, U2>::value;
};


template <typename T1, typename T2> struct 
is_matching<std::pair<T1, T2>, std::pair<T1, T2> > 
    : true_type {};


// value<T> matches if type matches
template <typename T1, typename Reader, typename T2> struct 
is_matching<value<T1, Reader>, T2>
    : is_matching<T1, T2> {};


// value<void> matches every container
template <typename T, typename Reader> struct 
is_matching<value<void, Reader>, T>
    : is_container<T> {};


template <typename T, typename X, typename Enable = void> struct
is_element_matching
    : false_type {};


template <typename T, typename X> struct
is_element_matching<T, X, typename boost::enable_if<is_container<X> >::type>
    : is_matching<T, typename element_type<X>::type> {};


template <typename X, typename Reader> struct
is_element_matching<value<void, Reader>, X, typename boost::enable_if<is_container<X> >::type>
{
    static const bool value = is_bond_type<typename element_type<X>::type>::value
                           || is_container<typename element_type<X>::type>::value;
};


template <typename T, typename X, typename Enable = void> struct
is_map_element_matching 
    : false_type {};


template <typename X, typename T> struct
is_map_element_matching<T, X, typename boost::enable_if<is_map_container<X> >::type>
    : is_matching<T, typename element_type<X>::type::second_type> {};


template <typename X, typename Reader> struct
is_map_element_matching<value<void, Reader>, X, typename boost::enable_if<is_map_container<X> >::type>
{
    static const bool value = is_bond_type<typename element_type<X>::type::second_type>::value
                           || is_container<typename element_type<X>::type::second_type>::value;
};


template <typename T, typename X, typename Enable = void> struct
is_map_key_matching 
    : false_type {};


template <typename T, typename X> struct
is_map_key_matching<T, X, typename boost::enable_if<is_map_container<X> >::type>
    : is_matching<T, typename element_type<X>::type::first_type> {};


template <typename T> struct 
is_basic_type
{
    static const bool value = !(is_container<T>::value || is_bond_type<T>::value);
};

template <> struct 
is_basic_type<void>
    : false_type {};


// is_nested_container
template <typename T, typename Enable = void> struct
is_nested_container 
    : false_type {};

template <typename T> struct
is_nested_container<T, typename boost::enable_if<is_map_container<T> >::type>
{
    static const bool value = !is_basic_type<typename element_type<T>::type::second_type>::value;
};

template <typename T> struct
is_nested_container<T, typename boost::enable_if<is_list_container<T> >::type>
{
    static const bool value = !is_basic_type<typename element_type<T>::type>::value;
};


template <typename T, typename Enable = void> struct
is_struct_container 
    : false_type {};

template <typename T> struct
is_struct_container<T, typename boost::enable_if<is_map_container<T> >::type>
{
    static const bool value = has_schema<typename element_type<T>::type::second_type>::value
                           || is_struct_container<typename element_type<T>::type::second_type>::value;
};


template <typename T> struct
is_struct_container<T, typename boost::enable_if<is_list_container<T> >::type>
{
    static const bool value = has_schema<typename element_type<T>::type>::value
                           || is_struct_container<typename element_type<T>::type>::value;
};


// is_struct_container_field
template <typename T> struct
is_struct_container_field
    : is_struct_container<typename T::field_type> {};


// is_basic_container
template <typename T, typename Enable = void> struct
is_basic_container 
    : false_type {};

template <typename T> struct
is_basic_container<T, typename boost::enable_if<is_map_container<T> >::type>
    : is_basic_type<typename element_type<T>::type::second_type> {};

template <typename T> struct
is_basic_container<T, typename boost::enable_if<is_list_container<T> >::type>
    : is_basic_type<typename element_type<T>::type> {};

template <typename T> struct
is_basic_container<T, typename boost::enable_if<is_set_container<T> >::type> 
    : true_type {};


template <typename T, typename F> struct 
is_matching_container_field
    : is_matching_container<T, typename F::field_type> {};


template <typename T> struct 
is_container_field
    : is_container<typename T::field_type> {};

template <typename T, typename F> struct 
is_matching_basic_field
    : is_matching_basic<T, typename F::field_type> {};

template<typename T, typename F, typename Enabled = void>
struct is_matching_field : is_matching_basic_field<T, F>
{};

template<typename T, typename F> struct
is_matching_field<T, F, typename std::enable_if<is_container_field<F>::value>::type> : is_matching_container_field<T, F> {};

template <typename T, typename Reader> struct 
is_basic_type<value<T, Reader> > 
    : false_type {};


template <typename T1, typename T2> struct 
is_basic_type<std::pair<T1, T2> > 
    : false_type {};

#if defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)

template <typename T, typename X, typename Enable = void> struct 
matching_fields
    : boost::mpl::copy_if<typename schema<T>::type::fields, 
                          is_matching_basic_field<X, _>, 
                          boost::mpl::front_inserter<boost::mpl::list<> > >
{
    BOOST_STATIC_ASSERT((is_basic_type<X>::value));
};


template <typename T, typename X> struct 
matching_fields<T, X, typename boost::enable_if<is_container<X> >::type>
    : boost::mpl::copy_if<typename schema<T>::type::fields, 
                          is_matching_container_field<X, _>, 
                          boost::mpl::front_inserter<boost::mpl::list<> > > {};


template <typename T> struct 
nested_fields
    : boost::mpl::copy_if<typename schema<T>::type::fields, 
                          is_nested_field<_>, 
                          boost::mpl::front_inserter<boost::mpl::list<> > > {};


template <typename T> struct 
struct_fields
    : boost::mpl::copy_if<typename schema<T>::type::fields, 
                          is_struct_field<_>, 
                          boost::mpl::front_inserter<boost::mpl::list<> > > {};


template <typename T> struct 
container_fields
    : boost::mpl::copy_if<typename schema<T>::type::fields, 
                          is_container_field<_>, 
                          boost::mpl::front_inserter<boost::mpl::list<> > > {};
#endif

template <typename T> struct 
has_base
    : has_schema<typename schema<T>::type::base> {};


template <typename T>
BondDataType 
GetTypeId(const T&)
{
    return get_type_id<T>::value;
}


template <typename Reader>
BondDataType 
GetTypeId(const value<void, Reader>& value)
{
    return value.GetTypeId();
}


template <typename T, typename Reader> struct 
get_type_id<value<T, Reader> >
    : get_type_id<T> {};


template <typename T1, typename T2> struct 
get_type_id<std::pair<T1, T2> >
{
    static const std::pair<BondDataType, BondDataType> value;
};

template <typename T1, typename T2>
const std::pair<BondDataType, BondDataType> 
get_type_id<std::pair<T1, T2> >::value = std::make_pair(
    get_type_id<typename remove_const<T1>::type>::value,
    get_type_id<T2>::value);

template <> struct 
get_type_id<bool>
    : boost::integral_constant<BondDataType, BondDataType::BT_BOOL> {};

template <> struct 
get_type_id<uint8_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_UINT8> {};

template <> struct 
get_type_id<uint16_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_UINT16> {};

template <> struct 
get_type_id<uint32_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_UINT32> {};

template <> struct 
get_type_id<uint64_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_UINT64> {};

template <> struct 
get_type_id<int8_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_INT8> {};

template <> struct 
get_type_id<int16_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_INT16> {};

template <> struct 
get_type_id<int32_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_INT32> {};

template <> struct 
get_type_id<int64_t>
    : boost::integral_constant<BondDataType, BondDataType::BT_INT64> {};

template <> struct 
get_type_id<float>
    : boost::integral_constant<BondDataType, BondDataType::BT_FLOAT> {};

template <> struct 
get_type_id<double>
    : boost::integral_constant<BondDataType, BondDataType::BT_DOUBLE> {};

template <> struct
get_type_id<void>
    : boost::integral_constant<BondDataType, BondDataType::BT_UNAVAILABLE> {};

template <typename T> struct 
get_type_id
{
    typedef typename remove_const<T>::type U;

    static const BondDataType value = 
        is_enum<T>::value           ? get_type_id<int32_t>::value :
        is_bond_type<T>::value      ? BondDataType::BT_STRUCT :
        is_set_container<U>::value  ? BondDataType::BT_SET :
        is_map_container<U>::value  ? BondDataType::BT_MAP :
        is_list_container<U>::value ? BondDataType::BT_LIST :
        is_string<U>::value         ? BondDataType::BT_STRING :
        is_wstring<U>::value        ? BondDataType::BT_WSTRING :
                                      get_type_id<typename aliased_type<T>::type>::value;
};

template <typename T> 
const BondDataType get_type_id<T>::value;

// class PrimitiveTypes
// {
//     struct Init
//     {
//         Init(uint32_t* _sizeof)
//             : _sizeof(_sizeof)
//         {}
//
//         template <typename T>
//         void operator()(const T&)
//         {
//             _sizeof[get_type_id<T>::value] = sizeof(T);
//         }
//
//         uint32_t* _sizeof;
//     };
//
// public:
//     typedef boost::mpl::list
//     <
//         bool, float, double,
//         uint8_t, uint16_t, uint32_t, uint64_t,
//         int8_t,  int16_t,  int32_t,  int64_t
//     >type;
//
//     PrimitiveTypes(uint32_t* _sizeof)
//     {
//         boost::mpl::for_each<type>(Init(_sizeof));
//     }
// };

} // namespace bond
