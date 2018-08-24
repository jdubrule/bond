
#pragma once

#include <bond/core/bond_version.h>

#if BOND_VERSION < 0x0800
#error This file was generated by a newer version of the Bond compiler and is incompatible with your version of the Bond library.
#endif

#if BOND_MIN_CODEGEN_VERSION > 0x0b03
#error This file was generated by an older version of the Bond compiler and is incompatible with your version of the Bond library.
#endif

#include <bond/core/config.h>
#include <bond/core/containers.h>



namespace test
{
    
    struct foo
    {
        std::map<std::string, int32_t> m;
        std::set<int32_t> s;
        
        foo()
        {
        }

        
        // Compiler generated copy ctor OK
        foo(const foo&) = default;
        
#if defined(_MSC_VER) && (_MSC_VER < 1900)  // Versions of MSVC prior to 1900 do not support = default for move ctors
        foo(foo&& other)
          : m(std::move(other.m)),
            s(std::move(other.s))
        {
        }
#else
        foo(foo&&) = default;
#endif
        
        
#if defined(_MSC_VER) && (_MSC_VER < 1900)  // Versions of MSVC prior to 1900 do not support = default for move ctors
        foo& operator=(foo other)
        {
            other.swap(*this);
            return *this;
        }
#else
        // Compiler generated operator= OK
        foo& operator=(const foo&) = default;
        foo& operator=(foo&&) = default;
#endif

        bool operator==(const foo& other) const
        {
            return true
                && (m == other.m)
                && (s == other.s);
        }

        bool operator!=(const foo& other) const
        {
            return !(*this == other);
        }

        void swap(foo& other)
        {
            using std::swap;
            swap(m, other.m);
            swap(s, other.s);
        }

        struct Schema;

    protected:
        void InitMetadata(const char*, const char*)
        {
        }
    };

    inline void swap(::test::foo& left, ::test::foo& right)
    {
        left.swap(right);
    }
} // namespace test
