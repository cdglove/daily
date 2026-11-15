// Copyright (c) 2026 Christopher Glover.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef DAILY_FAST_IOSTREAM_FASTLOCALE_HPP
#define DAILY_FAST_IOSTREAM_FASTLOCALE_HPP
#pragma once

#include <locale>

namespace daily {

// Returns the named locale with number formatting and parsing replaced by
// faster implementations that ignore the locale's number formatting rules.
std::locale generate_fast_locale(char const* loc_name);

} // namespace daily

#endif // DAILY_FAST_IOSTREAM_FASTLOCALE_HPP
