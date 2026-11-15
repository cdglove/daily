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
#ifndef DAILY_PROGRAM_OPTIONS_PROGRAMOPTIONS_HPP
#define DAILY_PROGRAM_OPTIONS_PROGRAMOPTIONS_HPP
#pragma once

#include <cstring>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace daily {

// Command line parsing for when Boost.ProgramOptions is more than needed.
// Each argument has the form key=value, and the value is read into the
// variable registered for the key with operator>>.
class ProgramOptions {
 public:
  // Registers destination to receive the value for key. Both must outlive
  // calls to parse.
  template <typename T>
  void add(char const* key, T& destination) {
    parsers_.push_back(std::make_unique<OptionHolder<T>>(key, destination));
  }

  // Returns false and sets error if an argument can't be parsed.
  bool parse(int argc, char** argv, std::string& error) {
    return parse_impl(argc, argv, &error);
  }

  // Throws std::runtime_error if an argument can't be parsed.
  void parse(int argc, char** argv) {
    parse_impl(argc, argv, nullptr);
  }

 private:
  struct OptionParser {
    explicit OptionParser(char const* key)
        : key(key) {
    }

    virtual ~OptionParser() = default;

    virtual bool parse_value(char const* value) = 0;

    char const* key;
  };

  template <typename Value>
  struct OptionHolder : OptionParser {
    OptionHolder(char const* key, Value& v)
        : OptionParser(key)
        , value(&v) {
    }

    bool parse_value(char const* str_val) override {
      if constexpr(std::is_same_v<Value, std::string>) {
        // Take the whole value rather than stopping at whitespace.
        *value = str_val;
        return true;
      }
      else {
        std::istringstream ins(str_val);
        ins >> std::boolalpha >> *value;
        // The whole value must be consumed.
        return !ins.fail() && (ins >> std::ws).eof();
      }
    }

    Value* value;
  };

  OptionParser* get_parser(char const* key, std::size_t key_len) {
    for(auto& p : parsers_) {
      if(std::strlen(p->key) == key_len && std::strncmp(p->key, key, key_len) == 0) {
        return p.get();
      }
    }

    return nullptr;
  }

  bool parse_impl(int argc, char** argv, std::string* error) {
    // Skip the program name.
    for(int i = 1; i < argc; ++i) {
      char const* key = argv[i];
      char const* val = std::strpbrk(argv[i], "=");
      OptionParser* p = nullptr;
      if(val) {
        p = get_parser(key, std::distance(key, val));
      }
      else {
        p = get_parser(key, std::strlen(key));
      }

      if(!p) {
        std::ostringstream e;
        e << "'" << key << "' is not a valid option.";
        if(error) {
          *error = e.str();
        }
        else {
          throw std::runtime_error(e.str());
        }

        return false;
      }

      if(!val) {
        std::ostringstream e;
        e << "'" << key << "' requires a value.";
        if(error) {
          *error = e.str();
        }
        else {
          throw std::runtime_error(e.str());
        }

        return false;
      }

      if(!p->parse_value(val + 1)) {
        std::ostringstream e;
        e << "Failed to parse value '" << (val + 1) << "' for key '" << p->key
          << "'.";
        if(error) {
          *error = e.str();
        }
        else {
          throw std::runtime_error(e.str());
        }

        return false;
      }
    }

    return true;
  }

  std::vector<std::unique_ptr<OptionParser>> parsers_;
};

} // namespace daily

#endif // DAILY_PROGRAM_OPTIONS_PROGRAMOPTIONS_HPP
