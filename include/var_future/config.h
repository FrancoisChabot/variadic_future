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

#ifndef AOM_VARIADIC_FUTURE_CONFIG_INCLUDED_H
#define AOM_VARIADIC_FUTURE_CONFIG_INCLUDED_H

#include <memory>
// **************************** std::expected ***************************//

// Change this if you want to use some other expected type.
#include "nonstd/expected.hpp"

namespace aom {
template <typename T>
using expected = nonstd::expected<T, std::exception_ptr>;
using unexpected = nonstd::unexpected_type<std::exception_ptr>;
}  // namespace aom

// ********************** No Undefined Behavior  *************************//

// There is currently (1) known technical UB in the implementation of the
// library. It is tolerated because it's more of a blind spot in the StdLib
// specification than an actual problem, and is consistent accross all
// compilers.
//
// Nevertheless, if you want absolute strict standard adherance,

//#define VAR_FUTURE_NO_UB

namespace aom {
// N.B. 1 vtable + 1 destination storage pointer + 1 function pointer.
static constexpr std::size_t var_fut_default_min_soo_size = 3 * sizeof(void*);

// A future's shared state can contain either a value, an error, or
// callbacks to invoke. When sizeof(value) is large, storing callbacks
// in small-object-optimization storage is trivial.
//
// However, when sizeof(value) is very small, like in a future<void>,
// most callbacks would end up on the heap, so we make future_storage a
// bit larger than strictly necessary in those cases to accomodate.
//
// This variable thus represents the size handlers guaranteed to be stored
// in SOO storage, which by default matches Future<void>::then([](){});
//
// The default value is used in tests, so it should be left alone.
static constexpr std::size_t var_fut_min_soo_size =
    var_fut_default_min_soo_size;
}  // namespace aom

#endif
