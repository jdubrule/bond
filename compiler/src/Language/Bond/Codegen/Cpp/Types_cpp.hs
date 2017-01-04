-- Copyright (c) Microsoft. All rights reserved.
-- Licensed under the MIT license. See LICENSE file in the project root for full license information.

{-# LANGUAGE QuasiQuotes, OverloadedStrings, RecordWildCards #-}

module Language.Bond.Codegen.Cpp.Types_cpp (types_cpp) where

import Data.Monoid
import Prelude
import Data.Text.Lazy (Text)
import Text.Shakespeare.Text
import Language.Bond.Syntax.Types
import Language.Bond.Codegen.TypeMapping
import Language.Bond.Codegen.Util
import qualified Language.Bond.Codegen.Cpp.Util as CPP

-- | Codegen template for generating /base_name/_types.cpp containing
-- definitions of helper functions and schema metadata static variables.
types_cpp :: MappingContext -> String -> [Import] -> [Declaration] -> (String, Text)
types_cpp cpp file _imports declarations = ("_types.cpp", [lt|
#include "#{file}_reflection.h"
#include <bond/core/exception.h>

#{CPP.openNamespace cpp}
    #{doubleLineSepEnd 1 statics declarations}
#{CPP.closeNamespace cpp}
|])
  where
    -- definitions of Schema statics for non-generic structs
    statics s@Struct {..} =
        if null declParams then CPP.schemaMetadata cpp s else mempty

    -- global variables for enum name/value conversions
    --
    -- ToString is intentionally not implemented in terms of FromEnum, as
    -- ToString returns a reference to the name stored in the map. FromEnum
    -- copies this name into the output paramater.
    statics Enum {..} = [lt|
    namespace _bond_enumerators
    {
    namespace #{declName}
    {
        const
        std::map<std::string, enum #{declName}> _name_to_value_#{declName} =
#if defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) || 1
            boost::assign::map_list_of<std::string, enum #{declName}>
                #{newlineSep 4 constantBoost enumConstants};
#else
            {
                #{newlineSep 4 constant enumConstants}
            };
#endif
        const
        std::map<enum #{declName}, std::string> _value_to_name_#{declName} =
            ::bond::reverse_map(_name_to_value_#{declName});

        const std::string& ToString(enum #{declName} value)
        {
            std::map<enum #{declName}, std::string>::const_iterator it =
                GetValueToNameMap(value).find(value);

            if (GetValueToNameMap(value).end() == it)
                ::bond::InvalidEnumValueException(value, "#{declName}");

            return it->second;
        }

        void FromString(const std::string& name, enum #{declName}& value)
        {
            if (!ToEnum(value, name))
                ::bond::InvalidEnumValueException(name.c_str(), "#{declName}");
        }
    } // namespace #{declName}
    } // namespace _bond_enumerators|]
      where
        constantBoost Constant {..} = [lt|("#{constantName}", #{constantName})|]
        constant Constant {..} = [lt|{"#{constantName}", #{constantName}},|]

    statics _ = mempty
