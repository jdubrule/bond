// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "reflection.h"
#include "detail/typeid_value.h"
#include "value.h"
#include "transforms.h"
#include "merge.h"
#include "schema.h"
#include "detail/inheritance.h"
#include <bond/protocol/simple_binary_impl.h>
#include <bond/protocol/simple_json_reader_impl.h>

namespace bond
{

namespace detail
{

class ParserCommon
{
protected:

    template <typename Fields>
    void SkipFields(const Fields&)
    {
    }

    template <typename TField>
    void SkipOneField(const TField&)
    {
    }

    template <typename T, typename Transform>
    typename boost::disable_if<is_fast_path_field<T, Transform>, bool>::type
    OmittedField(const T&, const Transform& transform)
    {
        return transform.OmittedField(T::id, T::metadata, get_type_id<typename T::field_type>::value);
    }


    template <typename T, typename Transform>
    typename boost::enable_if<is_fast_path_field<T, Transform>, bool>::type
    OmittedField(const T& field, const Transform& transform)
    {
        return transform.OmittedField(field);
    }

    template <typename Transform>
    struct UnknownFieldBinder
        : detail::nonassignable
    {
        UnknownFieldBinder(Transform& transform)
            : transform(transform)
        {}

        template <typename T>
        bool Field(uint16_t id, const Metadata& /*metadata*/, const T& value) const
        {
            return transform.UnknownField(id, value);
        }

        Transform& transform;
    };

    template <typename Transform>
    UnknownFieldBinder<Transform> BindUnknownField(Transform& transform)
    {
        return UnknownFieldBinder<Transform>(transform);
    }
};

} // namespace detail

//
// StaticParser iterates serialized data using type schema and calls 
// specified transform for each data field. 
// The schema may be provided at compile-time (schema<T>::type::fields) or at runtime 
// (const RuntimeSchema&). StaticParser is used with protocols which don't 
// tag fields in serialized format with ids or types, e.g. Apache Avro protocol.
//
template <typename Input>
class StaticParser
    : protected detail::ParserInheritance<Input, StaticParser<Input> >,
      public detail::ParserCommon
{
public:
    StaticParser(Input input, bool base = false)
        : detail::ParserInheritance<Input, StaticParser<Input> >(input, base)
    {}

    
    template <typename Schema, typename Transform>
    bool
    Apply(const Transform& transform, const Schema& schema)
    {                                               
        detail::StructBegin(_input, _base);
        bool result = this->Read(schema, transform);
        detail::StructEnd(_input, _base);
        return result;
    }

    friend class detail::ParserInheritance<Input, StaticParser<Input> >;


protected:
    using detail::ParserInheritance<Input, StaticParser<Input> >::_input;
    using detail::ParserInheritance<Input, StaticParser<Input> >::_base;

private:
    template <typename TField>
    void SkipOneField(const TField& field)
    {
        // Skip the structure by reading fields to Null transform
        ReadOneField(field, Null());
    }

    // use compile-time schema
    template <typename Transform>
    void
    BeginFields(const Transform&)
    {
    }

    template <typename t_field, typename Transform>
    bool
    ReadOneField(const t_field&, const Transform& transform)
    {
        if (detail::ReadFieldOmitted(_input))
            OmittedField(t_field(), transform);
        else
            NextField(t_field(), transform);

        return false; // Don't skip fields
    }

    template <typename Transform>
    void
    EndFields(const Transform&)
    {
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<detail::is_reader<Input>::value && !is_nested_field<T>::value
                             && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform)
    {
        return transform.Field(T::id, T::metadata, value<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<detail::is_reader<Input>::value && !is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform)
    {
        return transform.Field(field, value<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<detail::is_reader<Input>::value && is_nested_field<T>::value
                             && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform)
    {
        return transform.Field(T::id, T::metadata, bonded<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<detail::is_reader<Input>::value && is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform)
    {
        return transform.Field(field, bonded<typename T::field_type, Input>(_input));
    }

    template <typename T, typename Transform>
    typename boost::disable_if<detail::is_reader<Input, T>, bool>::type
    NextField(const T&, const Transform& transform)
    {
        return transform.Field(T::id, T::metadata, T::GetVariable(_input));
    }

    // use runtime schema
    void SkipFields(const RuntimeSchema& fields)
    {
        // Skip the structure by reading fields to Null transform
        ReadFields(fields, Null());
    }


    bool
    ReadRuntimeField(const RuntimeSchema& schema, const Transform& transform, const FieldDef& field)
    {}

    template<typename Transform>
    bool
    ReadRuntimeField(const RuntimeSchema& schema, const Transform &transform, const FieldDef &field)
    {
        if (detail::ReadFieldOmitted(_input))
        {
            transform.OmittedField(field.id, field.metadata, field.type.id);
            return false;
        }

        if (field.type.id == bond::BondDataType::BT_STRUCT)
        {
            return transform.Field(field.id, field.metadata, bonded<void, Input>(_input, RuntimeSchema(schema, field)));
        }
        else if (field.type.id == bond::BondDataType::BT_LIST || field.type.id == bond::BondDataType::BT_SET || field.type.id == bond::BondDataType::BT_MAP)
        {
            return transform.Field(field.id, field.metadata, value<void, Input>(_input, RuntimeSchema(schema, field)));
        }
        else
        {
            return detail::BasicTypeField(field.id, field.metadata, field.type.id, transform, _input);
        }
    }


    template <typename Transform>
    bool
    ReadFields(const RuntimeSchema& schema, const Transform& transform)
    {
        bool done = false;

        const_enumerator<std::vector<FieldDef> > enumerator(schema.GetStruct().fields);
        while (enumerator.more() && !done)
        {
            const FieldDef& field = enumerator.next();
            done = ReadRuntimeField(schema, transform, field);
        }

        // Skip trailing fields the transform didn't care about.
        while (enumerator.more())
        {
            const FieldDef& field = enumerator.next();

            ReadRuntimeField(schema, Null(), field);
        }

        return done;
    }
};


//
// DynamicParser iterates serialized data using field tags included in the
// data by the protocol and calls specified transform for each data field. 
// DynamicParser uses schema only for auxiliary metadata, such as field 
// names or modifiers, and determines what fields are present from the data itself. 
// The schema may be provided at compile-time (schema<T>::type::fields) or at runtime 
// (const RuntimeSchema&). 
// DynamicParser is used with protocols which tag fields in serialized  
// format with ids and types, e.g. Mafia, Thrift or Protocol Buffers.
//
template <typename Input>
class DynamicParser
    : protected detail::ParserInheritance<Input, DynamicParser<Input> >,
      public detail::ParserCommon
{
public:
    DynamicParser(Input input, bool base)
        : detail::ParserInheritance<Input, DynamicParser<Input> >(input, base)
    {}

    
    template <typename Schema, typename Transform>
    bool
    Apply(const Transform& transform, const Schema& schema)
    {                                               
        detail::StructBegin(_input, _base);
        bool result = this->Read(schema, transform);
        detail::StructEnd(_input, _base);
        return result;
    }

    friend class detail::ParserInheritance<Input, DynamicParser<Input> >;


protected:
    using detail::ParserInheritance<Input, DynamicParser<Input> >::_input;
    using detail::ParserInheritance<Input, DynamicParser<Input> >::_base;


private:
    uint16_t     _id = invalid_field_id;
    BondDataType _type;

    template <typename Transform>
    void
    BeginFields(const Transform&)
    {
        // Read the first type/ID, and begin processing.
        _input.ReadFieldBegin(_type, _id);
    }

    template <typename t_field, typename Transform>
    bool 
    ReadOneField(const t_field& field, const Transform& transform)
    {
        for (;;)
        {
            if (t_field::id == _id && get_type_id<typename t_field::field_type>::value == _type)
            {
                // Exact match
                NextField(field, transform);
            }
            else if (t_field::id >= _id && _type != bond::BondDataType::BT_STOP && _type != bond::BondDataType::BT_STOP_BASE)
            {
                // Unknown field or non-exact type match
                UnknownFieldOrTypeMismatch(field, _id, _type, transform);
            }
            else
            {
                OmittedField(field, transform);
                return false; // retry with the next field in the struct.
            }

            _input.ReadFieldEnd();
            _input.ReadFieldBegin(_type, _id);

            if (t_field::id < _id || _type == bond::BondDataType::BT_STOP || _type == bond::BondDataType::BT_STOP_BASE)
            {
                return false;
            }
        }
    }

    
    // use compile-time schema
    template <typename TField, typename Transform>
    bool
    ReadOneField(const TField&, uint16_t& id, BondDataType& type, const Transform& transform)
    {
        return false;
    }

    template <typename Transform>
    void
    EndFields(const Transform& transform)
    {
        // Read any data past the last known field to the end of the struct or the end of the current base-class.
        for (; _type != bond::BondDataType::BT_STOP && _type != bond::BondDataType::BT_STOP_BASE;
        _input.ReadFieldEnd(), _input.ReadFieldBegin(_type, _id))
        {
            UnknownField(_id, _type, transform);
        }

        if (!_base)
        {
            // If we are not parsing a base class, and we still didn't get to
            // the end of the struct, it means that:
            //
            // 1) Actual data in the payload had deeper hierarchy than payload schema.
            //
            // or
            //
            // 2) We parsed only part of the hierarchy because that was what
            //    the transform "expected".
            //
            // In both cases we emit remaining fields as unknown

            for (; _type != bond::BondDataType::BT_STOP; _input.ReadFieldEnd(), _input.ReadFieldBegin(_type, _id))
            {
                if (_type == bond::BondDataType::BT_STOP_BASE)
                    transform.UnknownEnd();
                else
                    UnknownField(_id, _type, transform);
            }
        }

        _input.ReadFieldEnd();
    }
 
    template <typename T, typename Transform>
    typename boost::enable_if_c<is_nested_field<T>::value
                            && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform)
    {
        return transform.Field(T::id, T::metadata, bonded<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform)
    {
        return transform.Field(field, bonded<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<!is_nested_field<T>::value
                             && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform)
    {
        return transform.Field(T::id, T::metadata, value<typename T::field_type, Input>(_input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<!is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform)
    {
        return transform.Field(field, value<typename T::field_type, Input>(_input));
    }


    // This function is called only when payload has unknown field id or type is not
    // matching exactly. This relatively rare so we don't inline the function to help
    // the compiler to optimize the common path. 
    template <typename T, typename Transform>
    BOND_NO_INLINE
    typename boost::enable_if<is_basic_type<typename T::field_type>, bool>::type
    UnknownFieldOrTypeMismatch(const T&, uint16_t id, BondDataType type, const Transform& transform)
    {
        if (id == T::id &&
            type != bond::BondDataType::BT_LIST &&
            type != bond::BondDataType::BT_SET &&
            type != bond::BondDataType::BT_MAP &&
            type != bond::BondDataType::BT_STRUCT)
        {
            return detail::BasicTypeField(T::id, T::metadata, type, transform, _input);
        }
        else
        {
            return UnknownField(id, type, transform);
        }
    }
    
    
    template <typename T, typename Transform>
    BOND_NO_INLINE
    typename boost::disable_if<is_basic_type<typename T::field_type>, bool>::type
    UnknownFieldOrTypeMismatch(const T&, uint16_t id, BondDataType type, const Transform& transform)
    {
        return UnknownField(id, type, transform);
    }
    
        
    // use runtime schema
    template <typename Transform>
    bool
    ReadFields(const RuntimeSchema& schema, const Transform& transform)
    {
        uint16_t                                id;
        BondDataType                            type;
        std::vector<FieldDef>::const_iterator   it = schema.GetStruct().fields.begin(),
                                                end = schema.GetStruct().fields.end();

        _input.ReadFieldBegin(type, id);

        for (;; _input.ReadFieldEnd(), _input.ReadFieldBegin(type, id))
        {
            while (it != end && (it->id < id || type == bond::BondDataType::BT_STOP || type == bond::BondDataType::BT_STOP_BASE))
            {
                const FieldDef& field = *it++;
                transform.OmittedField(field.id, field.metadata, field.type.id);
            }

            if (type == bond::BondDataType::BT_STOP || type == bond::BondDataType::BT_STOP_BASE)
            {
                break;
            }
            
            if (it != end && it->id == id)
            {
                const FieldDef& field = *it++;

                if (type == bond::BondDataType::BT_STRUCT)
                {
                    if (field.type.id == type)
                    {
                        transform.Field(id, field.metadata, bonded<void, Input>(_input, RuntimeSchema(schema, field)));
                        continue;
                    }
                }
                else if (type == bond::BondDataType::BT_LIST || type == bond::BondDataType::BT_SET || type == bond::BondDataType::BT_MAP)
                {
                    if (field.type.id == type)
                    {
                        transform.Field(id, field.metadata, value<void, Input>(_input, RuntimeSchema(schema, field)));
                        continue;
                    }
                }
                else
                {
                    detail::BasicTypeField(id, field.metadata, type, transform, _input);
                    continue;
                }
            }

            UnknownField(id, type, transform);
        }

        if (!_base)
        {
            // If we are not parsing a base class, and we still didn't get to
            // the end of the struct, it means that:
            // 
            // 1) Actual data in the payload had deeper hierarchy than payload schema.
            //
            // or
            //
            // 2) We parsed only part of the hierarchy because that was what
            //    the transform "expected".
            //
            // In both cases we emit remaining fields as unknown
            
            for (; type != bond::BondDataType::BT_STOP; _input.ReadFieldEnd(), _input.ReadFieldBegin(type, id))
            {
                if (type == bond::BondDataType::BT_STOP_BASE)
                {
                    transform.UnknownEnd();
                }
                else
                {
                    UnknownField(id, type, transform);
                }
            }
        }

        _input.ReadFieldEnd();

        return false;
    }


    template <typename T>
    bool UnknownField(uint16_t, BondDataType type, const To<T>&)
    {
        _input.Skip(type);
        return false;
    }


    template <typename Transform>
    bool UnknownField(uint16_t id, BondDataType type, const Transform& transform)
    {
        if (type == bond::BondDataType::BT_STRUCT)
        {
            return transform.UnknownField(id, bonded<void, Input>(_input, GetRuntimeSchema<Unknown>()));
        }
        else if (type == bond::BondDataType::BT_LIST || type == bond::BondDataType::BT_SET || type == bond::BondDataType::BT_MAP)
            return transform.UnknownField(id, value<void, Input>(_input, type));
        else
            return detail::BasicTypeField(id, schema<Unknown>::type::metadata, type, BindUnknownField(transform), _input);
    }
};


// DOM parser works with protocol implementations using Document Object Model, 
// e.g. JSON or XML. The parser assumes that fields in DOM are unordered and 
// are identified by either ordinal or metadata. DOM based protocols may loosely
// map to Bond meta-schema types thus the parser delegates to the protocol for 
// field type match checking. 
template <typename Input>
class DOMParser
    : protected detail::ParserInheritance<Input, DOMParser<Input> >
{
    typedef typename remove_reference<Input>::type Reader;

public:
    DOMParser(Input input, bool base = false)
        : detail::ParserInheritance<Input, DOMParser<Input> >(input, base)
    {}

    
    template <typename Schema, typename Transform>
    bool Apply(const Transform& transform, const Schema& schema)
    {
        if (!_base) _input.Parse();
        return this->Read(schema, transform);
    }

    friend class detail::ParserInheritance<Input, DOMParser<Input> >;


protected:
    using detail::ParserInheritance<Input, DOMParser<Input> >::_input;
    using detail::ParserInheritance<Input, DOMParser<Input> >::_base;

private:
    // use compile-time schema
    template <typename TField>
    void SkipOneField(const TField&)
    {}

    template<typename Transform>
    void BeginFields(const Transform&) {}

    template<typename TField, typename Transform>
    bool ReadOneField(const TField&, const Transform& transform)
    {
        if (const typename Reader::Field* field = _input.FindField(
                TField::id,
                TField::metadata,
                get_type_id<typename TField::field_type>::value,
                is_enum<typename TField::field_type>::value))
        {
            Reader input(_input, *field);
            NextField(TField(), transform, input);
        }

        return false;
    }

    template<typename Transform>
    void EndFields(const Transform&) {}

    template <typename T, typename Transform>
    typename boost::enable_if_c<is_nested_field<T>::value
                            && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform, Input input)
    {
        return transform.Field(T::id, T::metadata, bonded<typename T::field_type, Input>(input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform, Input input)
    {
        return transform.Field(field, bonded<typename T::field_type, Input>(input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<!is_nested_field<T>::value
                             && !is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T&, const Transform& transform, Input input)
    {
        return transform.Field(T::id, T::metadata, value<typename T::field_type, Input>(input));
    }


    template <typename T, typename Transform>
    typename boost::enable_if_c<!is_nested_field<T>::value
                             && is_fast_path_field<T, Transform>::value, bool>::type
    NextField(const T& field, const Transform& transform, Input input)
    {
        return transform.Field(field, value<typename T::field_type, Input>(input));
    }


    // use runtime schema
    template <typename Fields>
    void SkipFields(const Fields&)
    {}

    template <typename Transform>
    bool ReadFields(const RuntimeSchema& schema, const Transform& transform)
    {
        bool done = false;

        for (const_enumerator<std::vector<FieldDef> > enumerator(schema.GetStruct().fields); enumerator.more() && !done;)
        {
            const FieldDef& fieldDef = enumerator.next();
            
            if (const typename Reader::Field* field = _input.FindField(fieldDef.id, fieldDef.metadata, fieldDef.type.id))
            {
                Reader input(_input, *field);

                if (fieldDef.type.id == BondDataType::BT_STRUCT)
                    done = transform.Field(fieldDef.id, fieldDef.metadata, bonded<void, Input>(input, RuntimeSchema(schema, fieldDef)));
                else if (fieldDef.type.id == BondDataType::BT_LIST || fieldDef.type.id == BondDataType::BT_SET || fieldDef.type.id == BondDataType::BT_MAP)
                    done = transform.Field(fieldDef.id, fieldDef.metadata, value<void, Input>(input, RuntimeSchema(schema, fieldDef)));
                else
                    done = detail::BasicTypeField(fieldDef.id, fieldDef.metadata, fieldDef.type.id, transform, input);
            }
        }

        return done;
    }
};


} // namespace bond
