-- Copyright (c) Microsoft. All rights reserved.
-- Licensed under the MIT license. See LICENSE file in the project root for full license information.

{-# LANGUAGE QuasiQuotes, OverloadedStrings, RecordWildCards #-}

module Language.Bond.Codegen.Cpp.Reflection_h (reflection_h) where

import System.FilePath
import Data.Monoid
import Prelude
import Data.Text.Lazy (Text)
import qualified Data.Foldable as F
import Text.Shakespeare.Text
import Language.Bond.Syntax.Types
import Language.Bond.Codegen.TypeMapping
import Language.Bond.Codegen.Util
import qualified Language.Bond.Codegen.Cpp.Util as CPP

-- | Codegen template for generating /base_name/_reflection.h containing schema
-- metadata definitions.
reflection_h :: MappingContext -> String -> [Import] -> [Declaration] -> (String, Text)
reflection_h cpp file imports declarations = ("_reflection.h", [lt|
#pragma once

#include "#{file}_types.h"
#include <bond/core/reflection.h>
#if defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) || 1
#include <boost/assign.hpp>
#include <boost/assign/list_of.hpp>
#endif

#{newlineSepEnd 0 include imports}
#{CPP.openNamespace cpp}
    #{doubleLineSepEnd 1 schema declarations}
#{CPP.closeNamespace cpp}
|])
  where
    idl = MappingContext idlTypeMapping [] [] []  

    -- C++ type
    cppType = getTypeName cpp

    -- template for generating #include statement from import
    include (Import path) = [lt|#include "#{dropExtension path}_reflection.h"|]

    -- template for generating struct schema
    schema s@Struct {..} = [lt|//
    // #{declName}
    //
    #{CPP.template s}struct #{className}::Schema
    {
        typedef #{baseType structBase} base;

        static const ::bond::Metadata metadata;
        #{newlineBeginSep 2 fieldMetadata structFields}

        public: struct var
        {#{fieldTemplates structFields}};

        private:
#if defined(BOND_ENABLE_PRECXX11_MPL_SCHEMAS)
        typedef boost::mpl::list<> fields0;
        #{newlineSep 2 pushField reverseIndexedFields}

        public: typedef #{typename}fields#{length structFields}::type fields;
#endif

#if !defined(BOND_NO_CXX11_VARIADIC_TEMPLATES)
        public: static const size_t fieldCount = #{length structFields};
        template<size_t I> struct field {};
        #{newlineSep 2 fieldTemplateArray indexedFields}
        template<typename F> struct fieldIndex {};
        #{newlineSep 2 fieldIndexTemplateArray indexedFields}
#endif

        #{constructor}
        
        static ::bond::Metadata GetMetadata()
        {
            return ::bond::reflection::MetadataInit#{metadataInitArgs}("#{declName}", "#{getDeclTypeName idl s}",
                #{CPP.attributeInit declAttributes}
            );
        }
    };
    #{onlyTemplate $ CPP.schemaMetadata cpp s}|]
      where
        classParams = CPP.classParams s

        className = CPP.className s

        onlyTemplate x = if null declParams then mempty else x

        metadataInitArgsMpl = onlyTemplate [lt|<boost::mpl::list#{classParams} >|]
        metadataInitArgs = onlyTemplate [lt|#{classParams}|]

        typename = onlyTemplate [lt|typename |]

        -- constructor, generated only for struct templates
        constructor = onlyTemplate [lt|
        Schema()
        {
            // Force instantiation of template statics
            (void)metadata;
            #{newlineSep 3 static structFields}
        }|]
          where
            static Field {..} = [lt|(void)s_#{fieldName}_metadata;|]
        
        -- list of field names zipped with indexes
        indexedFields :: [(String, Int)]
        indexedFields = zipWith ((,) . fieldName) structFields [0..]

        -- reversed list of field names zipped with indexes
        reverseIndexedFields :: [(String, Int)]
        reverseIndexedFields = zipWith ((,) . fieldName) (reverse structFields) [0..]

        baseType (Just base) = cppType base
        baseType Nothing = "::bond::no_base"

        pushField (field, i) =
            [lt|private: typedef #{typename}boost::mpl::push_front<fields#{i}, #{typename}var::#{field}>::type fields#{i + 1};|]

        fieldTemplateArray (field, i) =
            [lt|template<> struct field<#{i}> { typedef #{typename} var::#{field} type; };|]

        fieldIndexTemplateArray (field, i) =
            [lt|template<> struct fieldIndex<#{typename} var::#{field}>: std::integral_constant<size_t, #{i}> {};|]

        fieldMetadata Field {..} =
            [lt|private: static const ::bond::Metadata s_#{fieldName}_metadata;|]

        fieldTemplates = F.foldMap $ \ f@Field {..} -> [lt|
            // #{fieldName}
            typedef ::bond::reflection::FieldTemplate<
                #{fieldOrdinal},
                #{CPP.modifierTag f},
                #{className},
                #{cppType fieldType},
                &#{className}::#{fieldName},
                &s_#{fieldName}_metadata
            > #{fieldName};
        |]


    -- nothing to generate for enums
    schema _ = mempty
