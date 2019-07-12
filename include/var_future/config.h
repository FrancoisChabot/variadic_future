// Copyright 2019 Age of Minds inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// \file
/// Configuration

#ifndef AOM_VARIADIC_FUTURE_CONFIG_INCLUDED_H
#define AOM_VARIADIC_FUTURE_CONFIG_INCLUDED_H

#define AOM_VARFUT_VERSION_MAJOR 0
#define AOM_VARFUT_VERSION_MINOR 3
#define AOM_VARFUT_VERSION_PATCH 2

// **************************** std::expected ***************************//

// Change this if you want to use some other expected type.
#include "nonstd/expected.hpp"

namespace aom {
template <typename T>
using expected = nonstd::expected<T, std::exception_ptr>;
using unexpected = nonstd::unexpected_type<std::exception_ptr>;
}  // namespace aom

#endif
