// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Defines an error domain that works out of the box. It combines some of the
// most useful extensions. See below for the list.
//
// To enable merror macros in a cc file, put the following using-declaration at
// the top of the file along with other using declarations you might have.
//
//   const auto MErrorDomain = merror::Default();
//
// Or a more concise form:
//
//   MERROR_DOMAIN(merror::Default);
//
// You can also put the declaration directly in the function that uses merror
// macros. This is a great choice for functions defined in headers.
//
//   Status DoStuff() {
//     const auto MErrorDomain = merror::Default();
//     ...
//   }
//
// Or:
//
//   Status DoStuff() {
//     MERROR_DOMAIN(merror::Default);
//     ...
//   }
//
// The default error domain exposes several knobs that alter its behavior. You
// can use them when declaring `MErrorDomain`.
//
//   const auto MErrorDomain = merror::Default().DefaultErrorCode(UNKNOWN);

#ifndef MERROR_5EDA97_DOMAIN_DEFAULT_H_
#define MERROR_5EDA97_DOMAIN_DEFAULT_H_

#include "merror/domain/adl_hooks.h"
#include "merror/domain/base.h"
#include "merror/domain/bool.h"
#include "merror/domain/description.h"
#include "merror/domain/error_passthrough.h"
#include "merror/domain/fill_error.h"
#include "merror/domain/forward.h"
#include "merror/domain/function.h"
#include "merror/domain/logging.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/optional.h"
#include "merror/domain/pointer.h"
#include "merror/domain/print.h"
#include "merror/domain/print_operands.h"
#include "merror/domain/return.h"
#include "merror/domain/status.h"
#include "merror/domain/tee.h"
#include "merror/domain/verify_via_try.h"

namespace merror {

namespace internal_default {

template <class Base>
using Policy = internal_status::AcceptStatus<internal_pointer::AcceptPointer<
    internal_function::AcceptFunction<internal_optional::AcceptOptional<
        internal_bool::AcceptBool<internal_logging::Policy<
            internal_status::StatusBuilder::Policy<internal_description::Policy<
                internal_tee::Policy<internal_return::Policy<
                    internal_forward::Facade<internal_print_operands::Printer<
                        internal_print::Policy<internal_method_hooks::Policy<
                            internal_adl_hooks::Policy<
                                internal_verify_via_try::Policy<
                                    Base>>>>>>>>>>>>>>>>;

template <class Base>
using Builder = internal_status::MakeStatus<
    internal_pointer::MakePointer<internal_function::MakeFunction<
        internal_optional::MakeOptional<internal_bool::MakeBool<
            internal_logging::Builder<internal_status::StatusBuilder::Builder<
                internal_description::Builder<
                    internal_tee::Builder<internal_return::Builder<
                        internal_print::Builder<internal_method_hooks::Builder<
                            internal_adl_hooks::Builder<
                                internal_status::MakeStatusFallback<
                                    internal_error_passthrough::Builder<
                                        internal_fill_error::Builder<
                                            Base>>>>>>>>>>>>>>>>;

}  // namespace internal_default

// An error domain that works out of the box.
//
// This declaration is equivalent to the following:
//
//   using Default = decltype(EmptyDomain().With(
//       // It's important for these extensions to come before AdlHooks() and
//       // MethodHooks(). Their relative order doesn't matter.
//       FillError(), ErrorPassthrough(), VerifyViaTry(), MakeStatusFallback(),
//       // Order matters here: method hooks trump ADL hooks.
//       AdlHooks(), MethodHooks(),
//       // For the remaining extensions order doesn't matter.
//       Print(), PrintOperands(), Forward(), Return(), Tee(),
//       DescriptionBuilder(), StatusBuilder(), Logging(), AcceptBool(),
//       MakeBool(), AcceptOptional(), MakeOptional(), AcceptFunction(),
//       MakeFunction(), AcceptPointer(), MakePointer(), AcceptStatus(),
//       MakeStatus(), FillRpc(), FillTask()));
//
// Its type is expanded to reduce compilation time.
using Default = Domain<internal_default::Policy, internal_default::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_DEFAULT_H_
