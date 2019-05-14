// See http://www.boost.org/libs/any for Documentation.

#ifndef BOOST_ANY_INCLUDED
#define BOOST_ANY_INCLUDED

#if defined(_MSC_VER)
# pragma once
#endif

// what:  variant type boost::any
// who:   contributed by Kevlin Henney,
//        with features contributed and bugs found by
//        Antony Polukhin, Ed Brey, Mark Rodgers, 
//        Peter Dimov, and James Curran
// when:  July 2001, April 2013 - 2019

#include <algorithm>

#include <boost/config.hpp>
#include <boost/aligned_storage.hpp>
#include <boost/type_index.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/decay.hpp>
#include <boost/type_traits/remove_cv.hpp>
#include <boost/type_traits/add_reference.hpp>
#include <boost/type_traits/is_reference.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include <boost/throw_exception.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/core/addressof.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/conditional.hpp>

namespace boost
{
    class any
    {
    private:
        enum operation
        {
            Destroy,
            Move,
            Copy,
            AnyCast,
            UnsafeCast,
            Typeinfo,
        };
        template <typename ValueType>
        static void* small_manager(operation op, any& left, const any* right, const boost::typeindex::type_info* info)
        {
            switch (op)
            {
                case Destroy:
                    reinterpret_cast<ValueType*>(&left.content.small_value)->~ValueType();
                    break;
                case Move:
                    new (&left.content.small_value) ValueType(std::move(*reinterpret_cast<ValueType*>(&const_cast<any*>(right)->content.small_value)));
                    left.man = right->man;
                    reinterpret_cast<ValueType const*>(&right->content.small_value)->~ValueType();
                    const_cast<any*>(right)->man = 0;
                    break;
                case Copy:
                    new (&left.content.small_value) ValueType(*reinterpret_cast<const ValueType*>(&right->content.small_value));
                    left.man = right->man;
                    break;
                case AnyCast:
                    return boost::typeindex::type_id<ValueType>() == *info ?
                            reinterpret_cast<BOOST_DEDUCED_TYPENAME remove_cv<ValueType>::type *>(&left.content.small_value) : 0;
                case UnsafeCast:
                    return reinterpret_cast<BOOST_DEDUCED_TYPENAME remove_cv<ValueType>::type *>(&left.content.small_value);
                case Typeinfo:
                    return const_cast<void*>(static_cast<const void*>(&boost::typeindex::type_id<ValueType>().type_info()));
                    break;
            }
            return 0;
        }

        template <typename ValueType>
        static void* large_manager(operation op, any& left, const any* right, const boost::typeindex::type_info* info)
        {
            switch (op)
            {
                case Destroy:
                    delete static_cast<ValueType*>(left.content.large_value);
                    break;
                case Move:
                    left.content.large_value = right->content.large_value;
                    left.man = right->man;
                    const_cast<any*>(right)->content.large_value = 0;
                    const_cast<any*>(right)->man = 0;
                    break;
                case Copy:
                    left.content.large_value = new ValueType(*static_cast<const ValueType*>(right->content.large_value));
                    left.man = right->man;
                    break;
                case AnyCast:
                    return boost::typeindex::type_id<ValueType>() == *info ?
                            static_cast<BOOST_DEDUCED_TYPENAME remove_cv<ValueType>::type *>(left.content.large_value) : 0;
                case UnsafeCast:
                    return reinterpret_cast<BOOST_DEDUCED_TYPENAME remove_cv<ValueType>::type *>(left.content.large_value);
                case Typeinfo:
                    return const_cast<void*>(static_cast<const void*>(&boost::typeindex::type_id<ValueType>().type_info()));
                    break;
            }
            return 0;
        }


        template <typename ValueType>
        struct is_small_object_impl : boost::integral_constant<bool, sizeof(ValueType) <= sizeof(void*) &&
            boost::alignment_of<ValueType>::value <= boost::alignment_of<void*>::value &&
            boost::is_nothrow_move_constructible<ValueType>::value>
        {};

        template <typename ValueType>
        struct is_small_object : is_small_object_impl<ValueType>
        {};

        template <typename ValueType, typename DecayedType = BOOST_DEDUCED_TYPENAME boost::decay<const ValueType>::type>
        static void create(any& any, const ValueType& value, boost::true_type)
        {
            any.man = &small_manager<DecayedType>;
            new (&any.content.small_value) ValueType(value);
        }

        template <typename ValueType, typename DecayedType = BOOST_DEDUCED_TYPENAME boost::decay<const ValueType>::type>
        static void create(any& any, const ValueType& value, boost::false_type)
        {
            any.man = &large_manager<DecayedType>;
            any.content.large_value = new DecayedType(value);
        }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
        template <typename ValueType, typename DecayedType = BOOST_DEDUCED_TYPENAME boost::decay<const ValueType>::type>
        static void create(any& any, ValueType&& value, boost::true_type)
        {
            any.man = &small_manager<DecayedType>;
            new (&any.content.small_value) DecayedType(static_cast<ValueType&&>(value));
        }

        template <typename ValueType, typename DecayedType = BOOST_DEDUCED_TYPENAME boost::decay<const ValueType>::type>
        static void create(any& any, ValueType&& value, boost::false_type)
        {
            any.man = &large_manager<DecayedType>;
            any.content.large_value = new DecayedType(static_cast<ValueType&&>(value));
        }
#endif
        public: // structors

        any() BOOST_NOEXCEPT
            : man(0)
        {
        }
template <typename T>
struct print;
        template<typename ValueType>
        any(const ValueType & value)
            : man(0)
        {
            create(*this, value, is_small_object<ValueType>());
        }

        any(const any & other)
          : man(0)
        {
            if (other.man)
            {
                other.man(Copy, *this, &other, 0);
            }
        }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
        // Move constructor
        any(any&& other) BOOST_NOEXCEPT
          : man(other.man)
        {
            man(Move, *this, &other, 0);
            other.man = 0;
        }

        // Perfect forwarding of ValueType
        template<typename ValueType>
        any(ValueType&& value
            , typename boost::disable_if<boost::is_same<any&, ValueType> >::type* = 0 // disable if value has type `any&`
            , typename boost::disable_if<boost::is_const<ValueType> >::type* = 0) // disable if value has type `const ValueType&&`
          : man(0)
        {
            create(*this, static_cast<ValueType&&>(value), is_small_object<BOOST_DEDUCED_TYPENAME boost::decay<ValueType>::type>());
        }
#endif

        ~any() BOOST_NOEXCEPT
        {
            if (man)
            {
                man(Destroy, *this, 0, 0);
            }
        }

    public: // modifiers

        any & swap(any & rhs) BOOST_NOEXCEPT
        {
            if (this != &rhs)
            {
                if (man && rhs.man)
                {
                    any tmp;
                    rhs.man(Move, tmp, &rhs, 0);
                    man(Move, rhs, this, 0);
                    tmp.man(Move, *this, &tmp, 0);
                }
                else if (man)
                {
                    man(Move, rhs, this, 0);
                }
                else if (rhs.man)
                {
                    rhs.man(Move, *this, &rhs, 0);
                }
            }
            return *this;
        }


#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
        template<typename ValueType>
        any & operator=(const ValueType & rhs)
        {
            any(rhs).swap(*this);
            return *this;
        }

        any & operator=(any rhs)
        {
            rhs.swap(*this);
            return *this;
        }

#else 
        any & operator=(const any& rhs)
        {
            any(rhs).swap(*this);
            return *this;
        }

        // move assignment
        any & operator=(any&& rhs) BOOST_NOEXCEPT
        {
            rhs.swap(*this);
            any().swap(rhs);
            return *this;
        }

        // Perfect forwarding of ValueType
        template <class ValueType>
        any & operator=(ValueType&& rhs)
        {
            any(static_cast<ValueType&&>(rhs)).swap(*this);
            return *this;
        }
#endif

    public: // queries

        bool empty() const BOOST_NOEXCEPT
        {
            return !man;
        }

        void clear() BOOST_NOEXCEPT
        {
            any().swap(*this);
        }

        const boost::typeindex::type_info& type() const BOOST_NOEXCEPT
        {
            return man
                    ? *static_cast<const boost::typeindex::type_info*>(man(Typeinfo, const_cast<any&>(*this), 0, 0))
                    : boost::typeindex::type_id<void>().type_info();
        }

#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS
    private: // types
#else
    public: // types (public so any_cast can be non-friend)
#endif

#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS

    private: // representation

        template<typename ValueType>
        friend ValueType * any_cast(any *) BOOST_NOEXCEPT;

        template<typename ValueType>
        friend ValueType * unsafe_any_cast(any *) BOOST_NOEXCEPT;

#else

    public: // representation (public so any_cast can be non-friend)

#endif
        typedef void*(*manager)(operation op, any& left, const any* right, const boost::typeindex::type_info* info);

        manager man;

        union content {
            BOOST_CONSTEXPR content() BOOST_NOEXCEPT
                : large_value(0) {}
            void * large_value;
            BOOST_DEDUCED_TYPENAME boost::aligned_storage<sizeof(void*), alignof(void*)>::type small_value;
        } content;

    };
 
    inline void swap(any & lhs, any & rhs) BOOST_NOEXCEPT
    {
        lhs.swap(rhs);
    }

    class BOOST_SYMBOL_VISIBLE bad_any_cast :
#ifndef BOOST_NO_RTTI
        public std::bad_cast
#else
        public std::exception
#endif
    {
    public:
        virtual const char * what() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return "boost::bad_any_cast: "
                   "failed conversion using boost::any_cast";
        }
    };

    template<typename ValueType>
    ValueType * any_cast(any * operand) BOOST_NOEXCEPT
    {
        return operand->man ?
                static_cast<BOOST_DEDUCED_TYPENAME remove_cv<ValueType>::type *>(operand->man(any::AnyCast, *operand, 0, &boost::typeindex::type_id<ValueType>().type_info()))
                : 0;
    }

    template<typename ValueType>
    inline const ValueType * any_cast(const any * operand) BOOST_NOEXCEPT
    {
        return any_cast<ValueType>(const_cast<any *>(operand));
    }

    template<typename ValueType>
    ValueType any_cast(any & operand)
    {
        typedef BOOST_DEDUCED_TYPENAME remove_reference<ValueType>::type nonref;


        nonref * result = any_cast<nonref>(boost::addressof(operand));
        if(!result)
            boost::throw_exception(bad_any_cast());

        // Attempt to avoid construction of a temporary object in cases when 
        // `ValueType` is not a reference. Example:
        // `static_cast<std::string>(*result);` 
        // which is equal to `std::string(*result);`
        typedef BOOST_DEDUCED_TYPENAME boost::conditional<
            boost::is_reference<ValueType>::value,
            ValueType,
            BOOST_DEDUCED_TYPENAME boost::add_reference<ValueType>::type
        >::type ref_type;

#ifdef BOOST_MSVC
#   pragma warning(push)
#   pragma warning(disable: 4172) // "returning address of local variable or temporary" but *result is not local!
#endif
        return static_cast<ref_type>(*result);
#ifdef BOOST_MSVC
#   pragma warning(pop)
#endif
    }

    template<typename ValueType>
    inline ValueType any_cast(const any & operand)
    {
        typedef BOOST_DEDUCED_TYPENAME remove_reference<ValueType>::type nonref;
        return any_cast<const nonref &>(const_cast<any &>(operand));
    }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<typename ValueType>
    inline ValueType any_cast(any&& operand)
    {
        BOOST_STATIC_ASSERT_MSG(
            boost::is_rvalue_reference<ValueType&&>::value /*true if ValueType is rvalue or just a value*/
            || boost::is_const< typename boost::remove_reference<ValueType>::type >::value,
            "boost::any_cast shall not be used for getting nonconst references to temporary objects" 
        );
        return any_cast<ValueType>(operand);
    }
#endif


    // Note: The "unsafe" versions of any_cast are not part of the
    // public interface and may be removed at any time. They are
    // required where we know what type is stored in the any and can't
    // use typeid() comparison, e.g., when our types may travel across
    // different shared libraries.
    template<typename ValueType>
    inline ValueType * unsafe_any_cast(any * operand) BOOST_NOEXCEPT
    {
        return static_cast<ValueType*>(operand->man(any::UnsafeCast, *operand, 0, 0));
    }

    template<typename ValueType>
    inline const ValueType * unsafe_any_cast(const any * operand) BOOST_NOEXCEPT
    {
        return unsafe_any_cast<ValueType>(const_cast<any *>(operand));
    }
}

// Copyright Kevlin Henney, 2000, 2001, 2002. All rights reserved.
// Copyright Antony Polukhin, 2013-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#endif
