// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

namespace bond { namespace ext { namespace gRPC { namespace detail {

    /// @brief Interface for completion queue tag types that \ref io_manager
    /// expects.
    ///
    /// Typically, a type inherits from this type, captures at construction
    /// time in its locals the state of some operation, and resumes that
    /// operation in its implementation of \ref invoke.
    struct io_manager_tag
    {
        virtual ~io_manager_tag() { }

        /// @brief Called when this instance is dequeued from a completion
        /// queue.
        ///
        /// @param ok whether or not the initial operation succeeded
        virtual void invoke(bool ok) = 0;
    };

} } } } // namespace bond::ext::gRPC::detail
