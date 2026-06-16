/**
 * @file filter_base.cpp
 * @brief Implementation of FilterBase abstract class
 *
 * This file contains implementation details that cannot be inlined.
 * The FilterBase class is abstract; concrete implementations inherit from it.
 */

#include "filter_base.h"

namespace audiofilter {

// FilterBase is a pure abstract class with only virtual methods.
// No implementation is needed here; this file exists for:
// 1. Consistency with C++ library structure
// 2. Future non-inline method implementations
// 3. Potential instantiation of any helper functions

// Virtual destructor implementation (required in .cpp for polymorphic classes)
// Implicitly generated, but shown here for clarity:
// FilterBase::~FilterBase() = default;

}  // namespace audiofilter
