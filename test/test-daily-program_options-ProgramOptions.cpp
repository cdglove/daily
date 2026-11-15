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
#include <boost/test/unit_test.hpp>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "daily/program_options/ProgramOptions.hpp"

namespace {

// Builds a mutable argv with a program name in front of args.
class Args {
 public:
  Args(std::initializer_list<char const*> args) {
    strings_.emplace_back("program");
    strings_.insert(strings_.end(), args.begin(), args.end());
    for(auto& s : strings_) {
      pointers_.push_back(s.data());
    }
  }

  int argc() const {
    return static_cast<int>(pointers_.size());
  }

  char** argv() {
    return pointers_.data();
  }

 private:
  std::vector<std::string> strings_;
  std::vector<char*> pointers_;
};

struct Options {
  Options() {
    options.add("--count", count);
    options.add("--name", name);
    options.add("--flag", flag);
    options.add("--ratio", ratio);
  }

  daily::ProgramOptions options;
  int count = 0;
  std::string name;
  bool flag = false;
  double ratio = 0;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(daily_program_options, Options)

BOOST_AUTO_TEST_CASE(no_arguments) {
  Args args{};
  std::string error;
  BOOST_TEST(options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(error.empty());
}

BOOST_AUTO_TEST_CASE(parses_values) {
  Args args{"--count=5", "--name=hello", "--flag=true", "--ratio=0.5"};
  std::string error;
  BOOST_TEST(options.parse(args.argc(), args.argv(), error), error);
  BOOST_TEST(count == 5);
  BOOST_TEST(name == "hello");
  BOOST_TEST(flag == true);
  BOOST_TEST(ratio == 0.5);
}

BOOST_AUTO_TEST_CASE(string_keeps_whole_value) {
  Args args{"--name=hello world"};
  std::string error;
  BOOST_TEST(options.parse(args.argc(), args.argv(), error), error);
  BOOST_TEST(name == "hello world");
}

BOOST_AUTO_TEST_CASE(empty_string_value) {
  name = "unchanged";
  Args args{"--name="};
  std::string error;
  BOOST_TEST(options.parse(args.argc(), args.argv(), error), error);
  BOOST_TEST(name.empty());
}

BOOST_AUTO_TEST_CASE(unknown_key) {
  Args args{"--other=1"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(error.find("--other") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(prefix_of_key_is_unknown) {
  Args args{"--co=1"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(count == 0);
}

BOOST_AUTO_TEST_CASE(key_with_suffix_is_unknown) {
  Args args{"--counter=1"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(count == 0);
}

BOOST_AUTO_TEST_CASE(missing_value) {
  Args args{"--count"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(!error.empty());
}

BOOST_AUTO_TEST_CASE(bad_value) {
  Args args{"--count=abc"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
  BOOST_TEST(error.find("abc") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(trailing_garbage) {
  Args args{"--count=5x"};
  std::string error;
  BOOST_TEST(!options.parse(args.argc(), args.argv(), error));
}

BOOST_AUTO_TEST_CASE(throwing_overload) {
  Args good{"--count=7"};
  BOOST_CHECK_NO_THROW(options.parse(good.argc(), good.argv()));
  BOOST_TEST(count == 7);

  Args bad{"--other=1"};
  BOOST_CHECK_THROW(options.parse(bad.argc(), bad.argv()), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
