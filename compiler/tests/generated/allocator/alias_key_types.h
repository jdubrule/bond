
#pragma once

#include <bond/core/bond_version.h>

#if BOND_VERSION < 0x0520
#error This file was generated by a newer version of the Bond compiler and is incompatible with your version of the Bond library.
#endif

#if BOND_MIN_CODEGEN_VERSION > 0x0a00
#error This file was generated by an older version of the Bond compiler and is incompatible with your version of the Bond library.
#endif

#include <bond/core/config.h>
#include <bond/core/containers.h>



namespace test
{
    
    struct foo
    {
        std::map<std::basic_string<char, std::char_traits<char>, typename arena::rebind<char>::other>, int32_t, std::less<std::basic_string<char, std::char_traits<char>, typename arena::rebind<char>::other> >, typename arena::rebind<std::pair<const std::basic_string<char, std::char_traits<char>, typename arena::rebind<char>::other>, int32_t> >::other> m;
        std::set<int32_t, std::less<int32_t>, typename arena::rebind<int32_t>::other> s;
        
        foo()
        {
        }

        
#ifndef BOND_NO_CXX11_DEFAULTED_FUNCTIONS
        // Compiler generated copy ctor OK
        foo(const foo&) = default;
#endif
        
#if !defined(BOND_NO_CXX11_DEFAULTED_MOVE_CTOR)
        foo(foo&&) = default;
#elif !defined(BOND_NO_CXX11_RVALUE_REFERENCES)
        foo(foo&& other)
          : m(std::move(other.m)),
            s(std::move(other.s))
        {
        }
#endif
        
        explicit
        foo(const arena& allocator)
          : m(std::less<std::basic_string<char, std::char_traits<char>, typename arena::rebind<char>::other>>(), allocator),
            s(std::less<int32_t>(), allocator)
        {
        }
        
        
#ifndef BOND_NO_CXX11_DEFAULTED_FUNCTIONS
        // Compiler generated operator= OK
        foo& operator=(const foo&) = default;
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

#if !defined(BOND_NO_CXX11_ALLOCATOR)
namespace std
{
    template <typename _Alloc>
    struct uses_allocator< ::test::foo, _Alloc>
        : is_convertible<_Alloc, arena>
    {};
}
#endif

