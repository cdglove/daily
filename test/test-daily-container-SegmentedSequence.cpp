// Copyright (c) 2026 Christopher Glover.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, sodtware
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <list>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "daily/container/SegmentedSequence.hpp"

BOOST_AUTO_TEST_CASE(SegmentedSequence_Construction) {
  daily::SegmentedSequence<int> table;
  BOOST_TEST(table.empty());
  BOOST_TEST(table.size() == 0);
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_PushBack) {
  daily::SegmentedSequence<int> table;
  table.push_back(1);
  BOOST_TEST(!table.empty());
  BOOST_TEST(table.size() == 1);
  BOOST_TEST(table.back() == 1);

  table.push_back(2);
  BOOST_TEST(table.size() == 2);
  BOOST_TEST(table.back() == 2);
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_PopBack) {
  daily::SegmentedSequence<int> table;
  table.push_back(1);
  table.push_back(2);
  BOOST_TEST(table.size() == 2);

  table.pop_back();
  BOOST_TEST(table.size() == 1);
  BOOST_TEST(table.back() == 1);

  table.pop_back();
  BOOST_TEST(table.empty());
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Clear) {
  daily::SegmentedSequence<int> table;
  table.push_back(1);
  table.push_back(2);
  BOOST_TEST(!table.empty());

  table.clear();
  BOOST_TEST(table.empty());
  BOOST_TEST(table.size() == 0);
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Iteration) {
  daily::SegmentedSequence<int> table;
  std::vector<int> values = {1, 2, 3, 4, 5};
  for(int v : values) {
    table.push_back(v);
  }

  BOOST_TEST(table.size() == values.size());

  int i = 0;
  for(auto& v : table) {
    BOOST_TEST(v == values[i++]);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Split) {
  daily::SegmentedSequence<int> table;
  const int num_elements = daily::SegmentedSequence<int>::kSplitThreshold + 1;
  for(int i = 0; i < num_elements; ++i) {
    table.push_back(i);
  }

  BOOST_TEST(table.size() == num_elements);

  int i = 0;
  for(int& v : table) {
    BOOST_TEST(v == i++);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Insert) {
  daily::SegmentedSequence<int> table;
  table.push_back(1);
  table.push_back(3);

  auto it = table.begin();
  ++it;
  table.insert(it, 2);

  BOOST_TEST(table.size() == 3);
  std::vector<int> expected = {1, 2, 3};
  int i = 0;
  for(auto& v : table) {
    BOOST_TEST(v == expected[i++]);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Erase) {
  daily::SegmentedSequence<int> table;
  table.push_back(1);
  table.push_back(2);
  table.push_back(3);

  auto it = table.begin();
  ++it;
  it = table.erase(it);

  BOOST_TEST(table.size() == 2);
  BOOST_TEST(*it == 3);

  std::vector<int> expected = {1, 3};
  int i = 0;
  for(auto& v : table) {
    BOOST_TEST(v == expected[i++]);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Merge) {
  daily::SegmentedSequence<int> table;
  const int num_elements = daily::SegmentedSequence<int>::kSplitThreshold;
  for(int i = 0; i < num_elements; ++i) {
    table.push_back(i);
  }

  // This should create two segments
  table.push_back(num_elements);
  BOOST_TEST(table.size() == num_elements + 1);

  // Erase enough elements to trigger a merge
  auto it = table.begin();
  for(int i = 0;
      i < num_elements - daily::SegmentedSequence<int>::kMergeThreshold + 1;
      ++i) {
    it = table.erase(it);
  }

  BOOST_TEST(table.size() == daily::SegmentedSequence<int>::kMergeThreshold);

  int i = num_elements - daily::SegmentedSequence<int>::kMergeThreshold + 1;
  for(auto& v : table) {
    BOOST_TEST(v == i++);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Copy) {
  daily::SegmentedSequence<int> table1;
  for(int i = 0; i < 100; ++i) {
    table1.push_back(i);
  }

  daily::SegmentedSequence<int> table2 = table1;
  BOOST_TEST(table1.size() == table2.size());
  auto it1 = table1.begin();
  auto it2 = table2.begin();
  for(; it1 != table1.end(); ++it1, ++it2) {
    BOOST_TEST(*it1 == *it2);
  }

  daily::SegmentedSequence<int> table3;
  table3 = table1;
  BOOST_TEST(table1.size() == table3.size());
  auto it3 = table3.begin();
  for(it1 = table1.begin(); it1 != table1.end(); ++it1, ++it3) {
    BOOST_TEST(*it1 == *it3);
  }
}

BOOST_AUTO_TEST_CASE(SegmentedSequence_Move) {
  daily::SegmentedSequence<int> table1;
  for(int i = 0; i < 100; ++i) {
    table1.push_back(i);
  }
  size_t original_size = table1.size();

  daily::SegmentedSequence<int> table2 = std::move(table1);
  BOOST_TEST(table1.empty());
  BOOST_TEST(table2.size() == original_size);

  int i = 0;
  for(auto& v : table2) {
    BOOST_TEST(v == i++);
  }

  daily::SegmentedSequence<int> table3;
  table3 = std::move(table2);
  BOOST_TEST(table2.empty());
  BOOST_TEST(table3.size() == original_size);

  i = 0;
  for(auto& v : table3) {
    BOOST_TEST(v == i++);
  }
}

BOOST_AUTO_TEST_SUITE(SegmentedSequence_SplitAndMerge)

BOOST_AUTO_TEST_CASE(SplitInMiddle) {
  daily::SegmentedSequence<int> table;
  // Create a single segment that is full
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    table.push_back(i);
  }

  // Insert in the middle to trigger a split
  auto it = table.begin();
  std::advance(it, table.size() / 2);
  table.insert(it, -1);

  BOOST_TEST(table.size() == daily::SegmentedSequence<int>::kSplitThreshold + 1);

  // Verify all elements are still there and in order
  it = table.begin();
  for(int i = 0; i < table.size() / 2; ++i, ++it) {
    BOOST_TEST(*it == i);
  }
  BOOST_TEST(*it == -1);
  ++it;
  for(int i = table.size() / 2; i < daily::SegmentedSequence<int>::kSplitThreshold;
      ++i, ++it) {
    BOOST_TEST(*it == i);
  }
}

BOOST_AUTO_TEST_CASE(MergeMiddle_MergeRight) {
  daily::SegmentedSequence<int> table;
  // 1. Create a full segment
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    table.push_back(i);
  }

  // 2. Create a second small segment to be the right-hand merge candidate.
  table.push_back(daily::SegmentedSequence<int>::kSplitThreshold);

  // 3. Split the first segment by inserting in the middle. This results in 3
  // segments. The middle segment will be the one that we shrink. The right
  // segment is smaller than the left, so the middle one should merge right.
  auto it = table.begin();
  std::advance(it, daily::SegmentedSequence<int>::kSplitThreshold / 2);
  table.insert(it, -1); // value -1 at index kSplitThreshold / 2

  const auto size_before_erase = table.size();
  BOOST_TEST(
      size_before_erase == daily::SegmentedSequence<int>::kSplitThreshold + 2);

  // 4. Erase elements from the middle segment to trigger a merge.
  // Middle segment size is kSplitThreshold / 2 + 1.
  // We need to bring it down to < kMergeThreshold.
  int elements_to_erase = (daily::SegmentedSequence<int>::kSplitThreshold / 2 + 1) -
                          (daily::SegmentedSequence<int>::kMergeThreshold - 1);

  it = table.begin();
  // advance to the beginning of the middle segment
  std::advance(it, daily::SegmentedSequence<int>::kSplitThreshold / 2);

  for(int i = 0; i < elements_to_erase; ++i) {
    it = table.erase(it);
  }

  BOOST_TEST(table.size() == size_before_erase - elements_to_erase);

  // Verify the contents of the table.
  std::vector<int> actual_values;
  for(int v : table) {
    actual_values.push_back(v);
  }

  std::vector<int> expected_values;
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    expected_values.push_back(i);
  }
  expected_values.push_back(daily::SegmentedSequence<int>::kSplitThreshold);
  expected_values.insert(
      expected_values.begin() + daily::SegmentedSequence<int>::kSplitThreshold / 2,
      -1);

  expected_values.erase(
      expected_values.begin() + daily::SegmentedSequence<int>::kSplitThreshold / 2,
      expected_values.begin() +
          daily::SegmentedSequence<int>::kSplitThreshold / 2 + elements_to_erase);

  BOOST_TEST(actual_values.size() == expected_values.size());
  BOOST_CHECK_EQUAL_COLLECTIONS(
      actual_values.begin(),
      actual_values.end(),
      expected_values.begin(),
      expected_values.end());
}

BOOST_AUTO_TEST_CASE(MergeMiddle_MergeLeft) {
  daily::SegmentedSequence<int> table;
  // 1. Create a full segment
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    table.push_back(i);
  }

  // 2. Create a second small segment
  table.push_back(daily::SegmentedSequence<int>::kSplitThreshold);

  // 3. Split the first segment by inserting in the middle. This results in 3
  // segments.
  auto it = table.begin();
  std::advance(it, daily::SegmentedSequence<int>::kSplitThreshold / 2);
  table.insert(it, -1);

  // 4. Grow the right-most segment so that it's larger than the left-most
  // segment. This will encourage a merge-left when the middle segment shrinks.
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold / 2 + 1; ++i) {
    table.push_back(daily::SegmentedSequence<int>::kSplitThreshold + 1 + i);
  }

  const auto size_before_erase = table.size();

  // 5. Erase elements from the middle segment to trigger a merge.
  // Middle segment size is kSplitThreshold / 2 + 1.
  // We need to bring it down to < kMergeThreshold.
  int elements_to_erase = (daily::SegmentedSequence<int>::kSplitThreshold / 2 + 1) -
                          (daily::SegmentedSequence<int>::kMergeThreshold - 1);

  it = table.begin();
  // advance to the beginning of the middle segment
  std::advance(it, daily::SegmentedSequence<int>::kSplitThreshold / 2);

  for(int i = 0; i < elements_to_erase; ++i) {
    it = table.erase(it);
  }

  BOOST_TEST(table.size() == size_before_erase - elements_to_erase);

  // Verify the contents of the table.
  std::vector<int> actual_values;
  for(int v : table) {
    actual_values.push_back(v);
  }

  std::vector<int> expected_values;
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    expected_values.push_back(i);
  }
  expected_values.push_back(daily::SegmentedSequence<int>::kSplitThreshold);
  expected_values.insert(
      expected_values.begin() + daily::SegmentedSequence<int>::kSplitThreshold / 2,
      -1);
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold / 2 + 1; ++i) {
    expected_values.push_back(
        daily::SegmentedSequence<int>::kSplitThreshold + 1 + i);
  }

  expected_values.erase(
      expected_values.begin() + daily::SegmentedSequence<int>::kSplitThreshold / 2,
      expected_values.begin() +
          daily::SegmentedSequence<int>::kSplitThreshold / 2 + elements_to_erase);

  BOOST_CHECK_EQUAL_COLLECTIONS(
      actual_values.begin(),
      actual_values.end(),
      expected_values.begin(),
      expected_values.end());
}

BOOST_AUTO_TEST_CASE(MergeAtBeginning) {
  daily::SegmentedSequence<int> table;
  const int num_elements_initial = daily::SegmentedSequence<int>::kSplitThreshold;
  for(int i = 0; i < num_elements_initial; ++i) {
    table.push_back(i);
  }

  // This should create two segments
  table.push_back(num_elements_initial);
  BOOST_TEST(table.size() == num_elements_initial + 1);

  // Erase enough elements from the first segment to trigger a merge
  auto it = table.begin();
  int erase_count = daily::SegmentedSequence<int>::kSplitThreshold -
                    (daily::SegmentedSequence<int>::kMergeThreshold - 1);
  for(int i = 0; i < erase_count; ++i) {
    it = table.erase(it);
  }

  BOOST_TEST(table.size() == num_elements_initial + 1 - erase_count);

  int i = erase_count;
  for(auto& v : table) {
    BOOST_TEST(v == i++);
  }
}

BOOST_AUTO_TEST_CASE(MergeAtEnd) {
  daily::SegmentedSequence<int> table;
  const int num_elements_initial = daily::SegmentedSequence<int>::kSplitThreshold;
  for(int i = 0; i < num_elements_initial; ++i) {
    table.push_back(i);
  }

  // This should create two segments
  table.push_back(num_elements_initial);
  BOOST_TEST(table.size() == num_elements_initial + 1);

  // Erase enough elements from the second segment to trigger a merge
  auto it = table.end();
  --it;

  // Erase the single element in the second segment, it will cause a merge
  table.erase(it);

  BOOST_TEST(table.size() == num_elements_initial);

  int i = 0;
  for(auto& v : table) {
    BOOST_TEST(v == i++);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_Bounds)

template <typename Container>
void TestBounds(Container& table, std::vector<int>& reference) {
  std::vector<int> test_values;
  if(!reference.empty()) {
    // A few values that exist
    test_values.push_back(reference.front());
    test_values.push_back(reference.back());
    test_values.push_back(reference[reference.size() / 2]);
    if(reference.size() > 4) {
      test_values.push_back(reference[reference.size() / 4]);
      test_values.push_back(reference[reference.size() * 3 / 4]);
    }

    // A few values that don't exist
    test_values.push_back(reference.front() - 1); // smaller than all
    test_values.push_back(reference.back() + 1);  // larger than all
    if(reference.size() > 1) {
      if(reference.front() + 1 < reference.back()) {
        test_values.push_back(reference.front() + 1); // between two elements
      }
      if(reference.back() - 1 > reference.front()) {
        test_values.push_back(reference.back() - 1);
      }
    }
    if(reference.size() > 2) {
      int mid_val = reference[reference.size() / 2];
      if(mid_val - 1 > reference.front()) {
        test_values.push_back(mid_val - 1);
      }
      if(mid_val + 1 < reference.back()) {
        test_values.push_back(mid_val + 1);
      }
    }
  }
  else {
    test_values.push_back(0);
    test_values.push_back(-1);
    test_values.push_back(1);
  }

  for(int v : test_values) {
    // Test lower_bound
    auto it_table = table.lower_bound(table.begin(), table.end(), v, std::less<int>());
    auto it_ref = std::lower_bound(reference.begin(), reference.end(), v);
    BOOST_TEST_INFO("Testing lower_bound for value " << v);
    BOOST_TEST(
        std::distance(reference.begin(), it_ref) ==
        std::distance(table.begin(), it_table));

    // Test upper_bound
    it_table = table.upper_bound(table.begin(), table.end(), v, std::less<int>());
    it_ref = std::upper_bound(reference.begin(), reference.end(), v);
    BOOST_TEST_INFO("Testing upper_bound for value " << v);
    BOOST_TEST(
        std::distance(reference.begin(), it_ref) ==
        std::distance(table.begin(), it_table));
  }
}

BOOST_AUTO_TEST_CASE(Bounds) {
  // Empty container
  daily::SegmentedSequence<int> empty_table;
  std::vector<int> empty_ref;
  TestBounds(empty_table, empty_ref);

  // Single segment
  daily::SegmentedSequence<int> single_segment_table;
  std::vector<int> single_segment_ref;
  for(int i = 0; i < 100; ++i) {
    single_segment_table.push_back(i * 2);
    single_segment_ref.push_back(i * 2);
  }
  TestBounds(single_segment_table, single_segment_ref);
  const auto& const_single_segment_table = single_segment_table;
  TestBounds(
      const_cast<daily::SegmentedSequence<int>&>(const_single_segment_table),
      single_segment_ref);

  // Multiple segments
  daily::SegmentedSequence<int> multi_segment_table;
  std::vector<int> multi_segment_ref;
  const int num_elements = daily::SegmentedSequence<int>::kSplitThreshold * 2 + 1;
  for(int i = 0; i < num_elements; ++i) {
    multi_segment_table.push_back(i * 3);
    multi_segment_ref.push_back(i * 3);
  }
  TestBounds(multi_segment_table, multi_segment_ref);
  const auto& const_multi_segment_table = multi_segment_table;
  TestBounds(
      const_cast<daily::SegmentedSequence<int>&>(const_multi_segment_table),
      multi_segment_ref);

  // Multiple segments with varied sizes
  daily::SegmentedSequence<int> varied_segment_table;
  std::vector<int> varied_segment_ref;
  // first segment full
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold; ++i) {
    varied_segment_table.push_back(i);
    varied_segment_ref.push_back(i);
  }
  // second segment half full
  for(int i = 0; i < daily::SegmentedSequence<int>::kSplitThreshold / 2; ++i) {
    varied_segment_table.push_back(
        daily::SegmentedSequence<int>::kSplitThreshold + i);
    varied_segment_ref.push_back(daily::SegmentedSequence<int>::kSplitThreshold + i);
  }
  // third segment with a few elements
  for(int i = 0; i < 10; ++i) {
    varied_segment_table.push_back(
        daily::SegmentedSequence<int>::kSplitThreshold * 3 / 2 + i);
    varied_segment_ref.push_back(
        daily::SegmentedSequence<int>::kSplitThreshold * 3 / 2 + i);
  }
  TestBounds(varied_segment_table, varied_segment_ref);
  const auto& const_varied_segment_table = varied_segment_table;
  TestBounds(
      const_cast<daily::SegmentedSequence<int>&>(const_varied_segment_table),
      varied_segment_ref);
}

BOOST_AUTO_TEST_CASE(BoundsOnSubrange) {
  daily::SegmentedSequence<int> table;
  std::vector<int> reference;
  const int num_elements = daily::SegmentedSequence<int>::kSplitThreshold * 2;
  for(int i = 0; i < num_elements; ++i) {
    table.push_back(i);
    reference.push_back(i);
  }

  // Create a sub-range for testing
  auto begin_it = table.begin();
  std::advance(begin_it, 10);
  auto end_it = table.begin();
  std::advance(end_it, daily::SegmentedSequence<int>::kSplitThreshold + 10);

  auto ref_begin_it = reference.begin() + 10;
  auto ref_end_it = reference.begin() +
                    daily::SegmentedSequence<int>::kSplitThreshold + 10;

  // Test a value that is within the first segment but before the sub-range start
  int value_before_begin = 5;
  auto it_table =
      table.lower_bound(begin_it, end_it, value_before_begin, std::less<int>());
  auto it_ref = std::lower_bound(ref_begin_it, ref_end_it, value_before_begin);

  BOOST_TEST_INFO(
      "Testing lower_bound on subrange for value before subrange begin");
  BOOST_TEST(
      std::distance(reference.begin(), it_ref) ==
      std::distance(table.begin(), it_table));
  // The result should be begin_it
  BOOST_TEST(std::distance(table.begin(), it_table) == 10);

  // Test a value within the sub-range
  int value_in_range = 20;
  it_table = table.lower_bound(begin_it, end_it, value_in_range, std::less<int>());
  it_ref = std::lower_bound(ref_begin_it, ref_end_it, value_in_range);
  BOOST_TEST_INFO("Testing lower_bound on subrange for value in range");
  BOOST_TEST(
      std::distance(reference.begin(), it_ref) ==
      std::distance(table.begin(), it_table));

  // Test a value that is within the last segment of the sub-range but after
  // the sub-range end
  int value_after_end = daily::SegmentedSequence<int>::kSplitThreshold + 20;
  it_table = table.lower_bound(begin_it, end_it, value_after_end, std::less<int>());
  it_ref = std::lower_bound(ref_begin_it, ref_end_it, value_after_end);
  BOOST_TEST_INFO(
      "Testing lower_bound on subrange for value after subrange end");
  BOOST_TEST(
      std::distance(reference.begin(), it_ref) ==
      std::distance(table.begin(), it_table));
  // The result should be end_it
  BOOST_TEST(it_table == end_it);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_Rotate)

void TestRotate(int container_size, int first_idx, int middle_idx, int last_idx) {
  daily::SegmentedSequence<int> table;
  std::vector<int> reference;
  for(int i = 0; i < container_size; ++i) {
    table.push_back(i);
    reference.push_back(i);
  }

  auto first = table.begin();
  std::advance(first, first_idx);
  auto middle = table.begin();
  std::advance(middle, middle_idx);
  auto last = table.begin();
  std::advance(last, last_idx);

  auto ref_first = reference.begin() + first_idx;
  auto ref_middle = reference.begin() + middle_idx;
  auto ref_last = reference.begin() + last_idx;

  auto ret = table.rotate(first, middle, last);
  auto ref_ret = std::rotate(ref_first, ref_middle, ref_last);

  BOOST_TEST_INFO(
      "Rotate test: size=" << container_size << ", first=" << first_idx
                           << ", middle=" << middle_idx << ", last=" << last_idx);
  BOOST_CHECK_EQUAL_COLLECTIONS(
      table.begin(),
      table.end(),
      reference.begin(),
      reference.end());

  BOOST_TEST(
      std::distance(table.begin(), ret) ==
      std::distance(reference.begin(), ref_ret));
}

BOOST_AUTO_TEST_CASE(RotateInSegment) {
  TestRotate(100, 10, 20, 30);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegments) {
  TestRotate(
      daily::SegmentedSequence<int>::kSplitThreshold * 2,
      10,
      daily::SegmentedSequence<int>::kSplitThreshold + 10,
      daily::SegmentedSequence<int>::kSplitThreshold * 2 - 10);
}

BOOST_AUTO_TEST_CASE(RotateFull) {
  TestRotate(100, 0, 50, 100);
}

BOOST_AUTO_TEST_CASE(RotateByOne) {
  TestRotate(100, 20, 21, 80);
}

BOOST_AUTO_TEST_CASE(RotateByNMinusOne) {
  TestRotate(100, 20, 79, 80);
}

BOOST_AUTO_TEST_CASE(RotateEmptyRange) {
  TestRotate(100, 10, 10, 10);
  TestRotate(100, 10, 20, 20);
  TestRotate(100, 10, 10, 20);
}

BOOST_AUTO_TEST_CASE(RotateToBeginning) {
  TestRotate(100, 0, 50, 100);
}

BOOST_AUTO_TEST_CASE(RotateToEnd) {
  TestRotate(100, 0, 100, 100);
}

BOOST_AUTO_TEST_SUITE_END()

namespace {

using IntTable = daily::SegmentedSequence<int>;
constexpr int kSplit = IntTable::kSplitThreshold;
constexpr int kMerge = IntTable::kMergeThreshold;

IntTable make_sequence(int count) {
  IntTable table;
  for(int i = 0; i < count; ++i) {
    table.push_back(i);
  }
  return table;
}

std::vector<int> make_reference(int count) {
  std::vector<int> reference;
  for(int i = 0; i < count; ++i) {
    reference.push_back(i);
  }
  return reference;
}

IntTable::iterator iterator_at(IntTable& table, int index) {
  return std::next(table.begin(), index);
}

// Checks the contents in both directions as well as size bookkeeping. Reports
// the first mismatching index rather than every mismatch because the tables
// are large.
void check_contents(IntTable const& table, std::vector<int> const& reference) {
  BOOST_TEST(table.size() == reference.size());
  BOOST_TEST(table.empty() == reference.empty());
  BOOST_TEST(
      std::distance(table.begin(), table.end()) ==
      static_cast<std::ptrdiff_t>(reference.size()));

  std::size_t forward_count = 0;
  auto ref_it = reference.begin();
  for(auto it = table.begin(); it != table.end(); ++it, ++ref_it, ++forward_count) {
    if(ref_it == reference.end()) {
      BOOST_ERROR("Table has more elements than the reference");
      return;
    }
    if(*it != *ref_it) {
      BOOST_ERROR(
          "Forward mismatch at index " << forward_count << ": " << *it
                                       << " != " << *ref_it);
      return;
    }
  }
  BOOST_TEST(forward_count == reference.size());

  std::size_t backward_count = 0;
  auto ref_rit = reference.rbegin();
  for(auto it = table.end(); it != table.begin(); ++ref_rit, ++backward_count) {
    --it;
    if(ref_rit == reference.rend()) {
      BOOST_ERROR("Table has more elements than the reference (reverse)");
      return;
    }
    if(*it != *ref_rit) {
      BOOST_ERROR(
          "Backward mismatch at index " << reference.size() - 1 - backward_count
                                        << ": " << *it << " != " << *ref_rit);
      return;
    }
  }
  BOOST_TEST(backward_count == reference.size());

  // Random access goes through the prefix sum of segment sizes, so this
  // checks it was kept up to date.
  auto const begin = table.begin();
  for(std::size_t i = 0; i < reference.size(); ++i) {
    auto it = begin + static_cast<std::ptrdiff_t>(i);
    if(*it != reference[i] || it - begin != static_cast<std::ptrdiff_t>(i)) {
      BOOST_ERROR(
          "Random access mismatch at index " << i << ": " << *it
                                             << " != " << reference[i]);
      return;
    }
  }
  BOOST_TEST(
      (begin + static_cast<std::ptrdiff_t>(reference.size()) == table.end()));
}

} // namespace

BOOST_AUTO_TEST_SUITE(SegmentedSequence_Iterators)

BOOST_AUTO_TEST_CASE(EmptyIteration) {
  IntTable table;
  BOOST_TEST((table.begin() == table.end()));
  BOOST_TEST((table.cbegin() == table.cend()));
  BOOST_TEST(std::distance(table.begin(), table.end()) == 0);
  check_contents(table, {});
}

BOOST_AUTO_TEST_CASE(IterationAcrossSegments) {
  int const count = kSplit * 2 + 7;
  check_contents(make_sequence(count), make_reference(count));
}

BOOST_AUTO_TEST_CASE(ReverseIteratorsAcrossSegments) {
  int const count = kSplit * 2 + 7;
  auto table = make_sequence(count);
  auto reference = make_reference(count);
  BOOST_TEST(
      std::equal(table.rbegin(), table.rend(), reference.rbegin(), reference.rend()));
}

BOOST_AUTO_TEST_CASE(DecrementFromSegmentStart) {
  auto table = make_sequence(kSplit * 2);
  auto it = iterator_at(table, kSplit);
  BOOST_TEST(*it == kSplit);
  --it;
  BOOST_TEST(*it == kSplit - 1);
  ++it;
  BOOST_TEST(*it == kSplit);
}

BOOST_AUTO_TEST_CASE(IncrementLastElementReachesEnd) {
  auto table = make_sequence(kSplit * 2 + 1);
  auto it = iterator_at(table, kSplit * 2);
  ++it;
  BOOST_TEST((it == table.end()));

  // Exactly full final segment.
  auto full = make_sequence(kSplit * 2);
  auto last = iterator_at(full, kSplit * 2 - 1);
  ++last;
  BOOST_TEST((last == full.end()));
}

BOOST_AUTO_TEST_CASE(Comparisons) {
  auto table = make_sequence(kSplit * 2 + 7);
  std::vector<int> indices =
      {0, 1, kSplit - 1, kSplit, kSplit + 1, kSplit * 2, kSplit * 2 + 6};
  for(int a : indices) {
    for(int b : indices) {
      auto ia = iterator_at(table, a);
      auto ib = iterator_at(table, b);
      BOOST_TEST_INFO("a=" << a << " b=" << b);
      BOOST_TEST((ia == ib) == (a == b));
      BOOST_TEST((ia != ib) == (a != b));
      BOOST_TEST((ia < ib) == (a < b));
      BOOST_TEST((ia <= ib) == (a <= b));
    }
    BOOST_TEST_INFO("a=" << a);
    BOOST_TEST((iterator_at(table, a) < table.end()));
  }
}

BOOST_AUTO_TEST_CASE(Distance) {
  int const count = kSplit * 3 + 7;
  auto table = make_sequence(count);
  std::vector<int> indices = {0, 1, kSplit - 1, kSplit, kSplit * 2 + 3, count - 1, count};
  for(int a : indices) {
    for(int b : indices) {
      if(a > b) {
        continue;
      }
      BOOST_TEST_INFO("a=" << a << " b=" << b);
      BOOST_TEST(
          std::distance(iterator_at(table, a), iterator_at(table, b)) == b - a);
    }
  }
}

BOOST_AUTO_TEST_CASE(AdvanceForward) {
  int const count = kSplit * 3 + 7;
  auto table = make_sequence(count);
  std::vector<int> starts = {0, 5, kSplit - 1, kSplit, kSplit + 5};
  std::vector<int> steps = {0, 1, 10, kSplit - 1, kSplit, kSplit + 1, kSplit * 2};
  for(int start : starts) {
    for(int step : steps) {
      int target = start + step;
      if(target > count) {
        continue;
      }
      auto it = iterator_at(table, start);
      it += step;
      BOOST_TEST_INFO("start=" << start << " step=" << step);
      if(target == count) {
        BOOST_TEST((it == table.end()));
      }
      else {
        BOOST_TEST(*it == target);
        BOOST_TEST((it == iterator_at(table, target)));
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(AdvanceToEnd) {
  int const count = kSplit * 2 + 7;
  auto table = make_sequence(count);
  auto it = table.begin();
  it += count;
  BOOST_TEST((it == table.end()));
}

BOOST_AUTO_TEST_CASE(AdvanceBackward) {
  int const count = kSplit * 3 + 7;
  auto table = make_sequence(count);
  std::vector<int> starts = {count, count - 1, kSplit * 2, kSplit + 5, kSplit, 10};
  std::vector<int> steps = {1, 5, 10, kSplit - 1, kSplit, kSplit + 1, kSplit * 2};
  for(int start : starts) {
    for(int step : steps) {
      int target = start - step;
      if(target < 0) {
        continue;
      }
      auto it = start == count ? table.end() : iterator_at(table, start);
      it += -step;
      BOOST_TEST_INFO("start=" << start << " step=-" << step);
      BOOST_TEST(*it == target);
    }
  }
}

BOOST_AUTO_TEST_CASE(ConstIteratorConversion) {
  auto table = make_sequence(kSplit + 10);
  IntTable::iterator it = iterator_at(table, kSplit + 2);

  IntTable::const_iterator constructed(it);
  BOOST_TEST(*constructed == kSplit + 2);

  IntTable::const_iterator assigned = table.cbegin();
  assigned = it;
  BOOST_TEST(*assigned == kSplit + 2);
  BOOST_TEST((assigned == constructed));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_Modifiers)

BOOST_AUTO_TEST_CASE(RangeConstructor) {
  auto reference = make_reference(kSplit * 2 + 3);
  IntTable table(reference.begin(), reference.end());
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(AssignReplacesContents) {
  auto table = make_sequence(kSplit * 2);
  auto reference = make_reference(10);
  table.assign(reference.begin(), reference.end());
  check_contents(table, reference);

  table.assign({7, 8, 9});
  check_contents(table, {7, 8, 9});
}

BOOST_AUTO_TEST_CASE(Swap) {
  auto a = make_sequence(kSplit + 1);
  IntTable b;
  b.push_back(42);
  a.swap(b);
  check_contents(a, {42});
  check_contents(b, make_reference(kSplit + 1));
}

// Members provided by sequence_container_interface in terms of SegmentedSequence's.
BOOST_AUTO_TEST_CASE(ContainerInterface) {
  auto table = make_sequence(kSplit + 1);
  auto reference = make_reference(kSplit + 1);
  IntTable const& ctable = table;
  BOOST_TEST(table.front() == 0);
  BOOST_TEST(ctable.back() == kSplit);
  BOOST_TEST(table[kSplit] == kSplit);
  BOOST_TEST(ctable.at(5) == 5);
  BOOST_CHECK_THROW(table.at(kSplit + 1), std::out_of_range);
  BOOST_TEST((table.cbegin() == ctable.begin()));
  BOOST_TEST(*table.crbegin() == kSplit);
  BOOST_TEST(
      std::equal(table.crbegin(), table.crend(), reference.crbegin(), reference.crend()));

  // Inserting an element of a full segment, which moves when it splits.
  table.insert(iterator_at(table, 10), table[kSplit - 1]);
  reference.insert(reference.begin() + 10, reference[kSplit - 1]);

  int const value = -1;
  table.push_back(value);
  table.insert(table.begin(), value);
  table.insert(table.cbegin() + 5, -2);
  table.emplace(table.cbegin() + 1, -3);
  BOOST_TEST(table.emplace_back(-4) == -4);
  reference.push_back(value);
  reference.insert(reference.begin(), value);
  reference.insert(reference.begin() + 5, -2);
  reference.emplace(reference.begin() + 1, -3);
  reference.emplace_back(-4);
  check_contents(table, reference);

  auto copy = table;
  BOOST_TEST((copy == table));
  copy.pop_back();
  BOOST_TEST((copy != table));
  BOOST_TEST((copy < table));
  swap(copy, table);
  BOOST_TEST(table.size() + 1 == copy.size());

  table.assign({1, 2, 3});
  check_contents(table, {1, 2, 3});
  IntTable from_range(reference.begin(), reference.end());
  check_contents(from_range, reference);
}

BOOST_AUTO_TEST_CASE(InsertReturnsInsertedElement) {
  auto table = make_sequence(10);

  auto it = table.insert(table.begin(), -1);
  BOOST_TEST(*it == -1);

  it = table.insert(iterator_at(table, 5), -2);
  BOOST_TEST(*it == -2);
  BOOST_TEST(std::distance(table.begin(), it) == 5);
}

BOOST_AUTO_TEST_CASE(InsertAtEndReturnsInsertedElement) {
  auto table = make_sequence(10);
  auto it = table.insert(table.end(), 100);
  BOOST_TEST((it != table.end()));
  BOOST_TEST(std::distance(table.begin(), it) == 10);
  BOOST_TEST(table.back() == 100);
}

BOOST_AUTO_TEST_CASE(InsertAtEndOfFullSegment) {
  auto table = make_sequence(kSplit);
  auto reference = make_reference(kSplit);
  auto it = table.insert(table.end(), -1);
  reference.push_back(-1);
  BOOST_TEST(std::distance(table.begin(), it) == kSplit);
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(InsertAtSegmentBoundaries) {
  auto table = make_sequence(kSplit * 2);
  auto reference = make_reference(kSplit * 2);

  // Both segments are full so each of these forces a split.
  for(int index : {kSplit, kSplit * 2 - 1, 0, kSplit / 2}) {
    auto it = table.insert(iterator_at(table, index), -index - 1);
    reference.insert(reference.begin() + index, -index - 1);
    BOOST_TEST_INFO("index=" << index);
    BOOST_TEST(*it == -index - 1);
    BOOST_TEST(std::distance(table.begin(), it) == index);
  }
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(EraseLastElementOfSegmentReturnsNext) {
  auto table = make_sequence(kSplit * 2 + 1);
  auto reference = make_reference(kSplit * 2 + 1);

  auto it = table.erase(iterator_at(table, kSplit - 1));
  reference.erase(reference.begin() + kSplit - 1);

  BOOST_TEST((it != table.end()));
  BOOST_TEST(*it == kSplit);
  BOOST_TEST((it == iterator_at(table, kSplit - 1)));
  ++it;
  BOOST_TEST(*it == kSplit + 1);
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(EraseLastElementAfterMergeLeftReturnsNext) {
  // Three segments. Shrink the middle one from the back so that the final
  // erase triggers a merge to the left while erasing the last element of
  // the segment.
  auto table = make_sequence(kSplit * 2);
  table.insert(iterator_at(table, kSplit / 2), -1);
  // Segments are now [kSplit/2][kSplit/2 + 1][kSplit]
  auto reference = make_reference(kSplit * 2);
  reference.insert(reference.begin() + kSplit / 2, -1);

  // Erase from the back of the middle segment until it merges.
  int middle_end = kSplit + 1;
  int middle_begin = kSplit / 2;
  while(middle_end - middle_begin >= kMerge - 1) {
    auto it = table.erase(iterator_at(table, middle_end - 1));
    reference.erase(reference.begin() + middle_end - 1);
    --middle_end;
    BOOST_TEST_INFO("middle_end=" << middle_end);
    BOOST_TEST(*it == reference[middle_end]);
    BOOST_TEST(std::distance(table.begin(), it) == middle_end);
  }
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(EraseEntireMiddleSegment) {
  // Four segments: three full and one with a single element. Full neighbours
  // prevent the middle segment from merging so it is eventually emptied.
  auto table = make_sequence(kSplit * 3 + 1);
  auto reference = make_reference(kSplit * 3 + 1);

  auto it = iterator_at(table, kSplit);
  for(int i = 0; i < kSplit; ++i) {
    it = table.erase(it);
    reference.erase(reference.begin() + kSplit);
    // Check the position as well as the value. An iterator whose segment
    // and element disagree can still dereference to the right value.
    if(*it != reference[kSplit] || std::distance(table.begin(), it) != kSplit ||
       *std::next(it) != reference[kSplit + 1]) {
      BOOST_ERROR(
          "Erase returned " << *it << " at " << std::distance(table.begin(), it)
                            << " expected " << reference[kSplit] << " at "
                            << kSplit << " after " << i + 1 << " erasures");
      break;
    }
  }

  // Keep erasing through the returned iterator. An iterator whose segment
  // and element disagree corrupts the table here even if it happened to
  // dereference correctly above.
  // Only kSplit + 1 elements remain from index kSplit, leave the last one.
  for(int i = 0; i < kSplit; ++i) {
    it = table.erase(it);
    reference.erase(reference.begin() + kSplit);
  }
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(EraseFromBackReturnsEnd) {
  int const count = kSplit * 2 + 3;
  auto table = make_sequence(count);
  for(int i = count - 1; i >= 0; --i) {
    auto it = table.erase(std::prev(table.end()));
    if(it != table.end()) {
      BOOST_ERROR("Erase of last element did not return end at " << i);
      break;
    }
  }
  check_contents(table, {});
}

BOOST_AUTO_TEST_CASE(EraseAllThenReuse) {
  auto table = make_sequence(kSplit * 3);
  auto it = table.begin();
  while(it != table.end()) {
    it = table.erase(it);
  }
  check_contents(table, {});
  BOOST_TEST((table.begin() == table.end()));

  table.push_back(1);
  table.insert(table.begin(), 0);
  check_contents(table, {0, 1});
}

BOOST_AUTO_TEST_CASE(PopBackAcrossSegments) {
  int const count = kSplit * 2 + 3;
  auto table = make_sequence(count);
  auto reference = make_reference(count);
  while(!reference.empty()) {
    BOOST_TEST(table.back() == reference.back());
    table.pop_back();
    reference.pop_back();
  }
  check_contents(table, reference);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_RotateEdgeCases)

void TestRotateFull(int container_size, int first_idx, int middle_idx, int last_idx) {
  auto table = make_sequence(container_size);
  auto reference = make_reference(container_size);

  auto ret = table.rotate(
      iterator_at(table, first_idx),
      iterator_at(table, middle_idx),
      iterator_at(table, last_idx));
  auto ref_ret = std::rotate(
      reference.begin() + first_idx,
      reference.begin() + middle_idx,
      reference.begin() + last_idx);

  BOOST_TEST_INFO(
      "Rotate test: size=" << container_size << ", first=" << first_idx
                           << ", middle=" << middle_idx << ", last=" << last_idx);
  BOOST_TEST(
      std::distance(table.begin(), ret) ==
      std::distance(reference.begin(), ref_ret));
  check_contents(table, reference);

  // The container must remain usable after the rotation.
  table.insert(table.begin(), -1);
  reference.insert(reference.begin(), -1);
  table.erase(iterator_at(table, container_size / 2));
  reference.erase(reference.begin() + container_size / 2);
  table.push_back(-2);
  reference.push_back(-2);
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegmentsToEnd) {
  TestRotateFull(kSplit * 2 + 5, 10, kSplit + 10, kSplit * 2 + 5);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegmentsWholeContainer) {
  TestRotateFull(kSplit * 3 + 1, 0, kSplit + 5, kSplit * 3 + 1);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegmentsMiddleEqualsLast) {
  TestRotateFull(kSplit * 2, 10, kSplit + 10, kSplit + 10);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegmentsFirstEqualsMiddle) {
  TestRotateFull(kSplit * 2, 10, 10, kSplit + 10);
}

BOOST_AUTO_TEST_CASE(RotateAcrossSegmentsFirstEqualsMiddleToEnd) {
  TestRotateFull(kSplit * 2 + 5, 10, 10, kSplit * 2 + 5);
}

BOOST_AUTO_TEST_CASE(RotateAlignedSegmentBoundaries) {
  TestRotateFull(kSplit * 3, 0, kSplit, kSplit * 2);
  TestRotateFull(kSplit * 3, kSplit, kSplit * 2, kSplit * 3);
}

BOOST_AUTO_TEST_CASE(RotateSmallRangeSpanningBoundary) {
  TestRotateFull(kSplit * 2, kSplit - 3, kSplit - 1, kSplit + 2);
}

BOOST_AUTO_TEST_CASE(RepeatedRotations) {
  // Directory moves repeatedly rotate ranges. Make sure the structure
  // survives many of them.
  int const count = kSplit * 3 + 11;
  auto table = make_sequence(count);
  auto reference = make_reference(count);
  std::mt19937 rng(1234);
  for(int i = 0; i < 200; ++i) {
    std::array<int, 3> idx = {
        std::uniform_int_distribution<int>(0, count)(rng),
        std::uniform_int_distribution<int>(0, count)(rng),
        std::uniform_int_distribution<int>(0, count)(rng)};
    std::sort(idx.begin(), idx.end());
    table.rotate(
        iterator_at(table, idx[0]),
        iterator_at(table, idx[1]),
        iterator_at(table, idx[2]));
    std::rotate(
        reference.begin() + idx[0],
        reference.begin() + idx[1],
        reference.begin() + idx[2]);
    if(!std::equal(table.begin(), table.end(), reference.begin(), reference.end())) {
      BOOST_ERROR(
          "Mismatch after rotation " << i << " (" << idx[0] << ", " << idx[1]
                                     << ", " << idx[2] << ")");
      break;
    }
  }
  check_contents(table, reference);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_Randomized)

// Mimics how an Ordinal uses the container: a sorted sequence that sees
// inserts at upper_bound, erases at lower_bound and in place repositioning
// of a changed element. The table grows past several segments and shrinks
// back down to exercise splitting and merging.
BOOST_AUTO_TEST_CASE(SortedOperationsMatchReference) {
  IntTable table;
  std::vector<int> reference;
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> value_dist(0, 1000000);

  auto check_bounds = [&](int value) {
    auto lb = table.lower_bound(table.begin(), table.end(), value, std::less<int>());
    auto ref_lb = std::lower_bound(reference.begin(), reference.end(), value);
    BOOST_TEST(std::distance(table.begin(), lb) == ref_lb - reference.begin());
    auto ub = table.upper_bound(table.begin(), table.end(), value, std::less<int>());
    auto ref_ub = std::upper_bound(reference.begin(), reference.end(), value);
    BOOST_TEST(std::distance(table.begin(), ub) == ref_ub - reference.begin());
  };

  int const peak = kSplit * 4 + 123;
  int op = 0;
  bool insert_error_reported = false;
  for(int cycle = 0; cycle < 2; ++cycle) {
    // Grow, with occasional erasures and repositions.
    while(static_cast<int>(reference.size()) < peak) {
      int value = value_dist(rng);
      auto it = table.upper_bound(table.begin(), table.end(), value, std::less<int>());
      auto inserted = table.insert(it, value);
      reference.insert(
          std::upper_bound(reference.begin(), reference.end(), value),
          value);
      if(!insert_error_reported && (inserted == table.end() || *inserted != value)) {
        // Report once. InsertAtEndReturnsInsertedElement covers the details
        // and the rest of this test is still useful.
        BOOST_ERROR("insert returned the wrong element at op " << op);
        insert_error_reported = true;
      }

      if(op % 7 == 0 && !reference.empty()) {
        int victim = reference[rng() % reference.size()];
        auto pos =
            table.lower_bound(table.begin(), table.end(), victim, std::less<int>());
        table.erase(pos);
        reference.erase(
            std::lower_bound(reference.begin(), reference.end(), victim));
      }

      if(op % 11 == 0 && !reference.empty()) {
        // Reposition: change a value and move it to its new sorted location.
        int old_value = reference[rng() % reference.size()];
        int new_value = value_dist(rng);
        auto old_pos = table.lower_bound(
            table.begin(),
            table.end(),
            old_value,
            std::less<int>());
        table.erase(old_pos);
        auto new_pos = table.upper_bound(
            table.begin(),
            table.end(),
            new_value,
            std::less<int>());
        table.insert(new_pos, new_value);
        reference.erase(
            std::lower_bound(reference.begin(), reference.end(), old_value));
        reference.insert(
            std::upper_bound(reference.begin(), reference.end(), new_value),
            new_value);
      }

      if(op % 5000 == 0) {
        check_contents(table, reference);
        check_bounds(value_dist(rng));
      }
      ++op;
    }

    check_contents(table, reference);

    // Shrink to empty by erasing random elements.
    while(!reference.empty()) {
      int victim = reference[rng() % reference.size()];
      auto pos =
          table.lower_bound(table.begin(), table.end(), victim, std::less<int>());
      if(pos == table.end() || *pos != victim) {
        BOOST_ERROR("lower_bound failed to find " << victim << " at op " << op);
        return;
      }
      table.erase(pos);
      reference.erase(std::lower_bound(reference.begin(), reference.end(), victim));
      if(op % 5000 == 0) {
        check_contents(table, reference);
        check_bounds(value_dist(rng));
      }
      ++op;
    }
    check_contents(table, reference);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_RandomAccess)

static_assert(std::random_access_iterator<IntTable::iterator>);
static_assert(std::random_access_iterator<IntTable::const_iterator>);

namespace {

// A table with segments of varied sizes: full ones from push_back, halves
// from splits and small ones from rotation.
IntTable make_varied_table(std::vector<int>& reference) {
  int const count = kSplit * 3 + 17;
  auto table = make_sequence(count);
  reference = make_reference(count);
  for(int index : {kSplit / 3, kSplit * 2 + 5, 7}) {
    table.insert(iterator_at(table, index), -index);
    reference.insert(reference.begin() + index, -index);
  }
  table.rotate(
      iterator_at(table, 100),
      iterator_at(table, kSplit + 200),
      iterator_at(table, kSplit * 2 + 300));
  std::rotate(
      reference.begin() + 100,
      reference.begin() + kSplit + 200,
      reference.begin() + kSplit * 2 + 300);
  return table;
}

} // namespace

BOOST_AUTO_TEST_CASE(RandomJumps) {
  std::vector<int> reference;
  auto table = make_varied_table(reference);
  check_contents(table, reference);

  auto const size = static_cast<int>(reference.size());
  std::mt19937 rng(7);
  std::uniform_int_distribution<int> index_dist(0, size);
  for(int i = 0; i < 20000; ++i) {
    int from = index_dist(rng);
    int to = index_dist(rng);
    auto it = table.begin() + from;
    it += to - from;
    if(it - table.begin() != to) {
      BOOST_ERROR(
          "Jump from " << from << " to " << to << " landed at "
                       << (it - table.begin()));
      return;
    }
    if(to == size) {
      BOOST_TEST((it == table.end()));
    }
    else if(*it != reference[to] || table.begin()[to] != reference[to]) {
      BOOST_ERROR("Jump from " << from << " to " << to << " read the wrong element");
      return;
    }

    // Iterators compare in index order.
    auto other = table.begin() + from;
    BOOST_TEST((other < it) == (from < to));
    BOOST_TEST((other == it) == (from == to));
    BOOST_TEST((it - other) == to - from);
  }
}

BOOST_AUTO_TEST_CASE(JumpsWithUnevenSegments) {
  // Jumps guess the target segment from the average segment size. Rotating
  // small blocks leaves tiny segments between large ones, so guesses are
  // wrong by many segments in both directions.
  int const count = kSplit * 6;
  auto table = make_sequence(count);
  auto reference = make_reference(count);
  std::mt19937 rng(23);
  for(int i = 0; i < 300; ++i) {
    int block = 1 + static_cast<int>(rng() % 50);
    int first = static_cast<int>(rng() % (count - block));
    int target = static_cast<int>(rng() % (count + 1));
    if(target >= first && target <= first + block) {
      continue;
    }
    if(target > first) {
      table.rotate(
          table.begin() + first,
          table.begin() + first + block,
          table.begin() + target);
      std::rotate(
          reference.begin() + first,
          reference.begin() + first + block,
          reference.begin() + target);
    }
    else {
      table.rotate(
          table.begin() + target,
          table.begin() + first,
          table.begin() + first + block);
      std::rotate(
          reference.begin() + target,
          reference.begin() + first,
          reference.begin() + first + block);
    }
  }
  // Every index from begin(), and backwards from end().
  check_contents(table, reference);
  for(int i = 1; i <= count; ++i) {
    if(*(table.end() - i) != reference[count - i]) {
      BOOST_ERROR("end() - " << i << " read the wrong element");
      return;
    }
  }

  std::uniform_int_distribution<int> index_dist(0, count);
  for(int i = 0; i < 50000; ++i) {
    int from = index_dist(rng);
    int to = index_dist(rng);
    auto it = table.begin() + from;
    it += to - from;
    if(it - table.begin() != to || (to < count && *it != reference[to])) {
      BOOST_ERROR("Jump from " << from << " to " << to << " went wrong");
      return;
    }
  }
}

BOOST_AUTO_TEST_CASE(JumpsOnEmptyAndSingleElement) {
  IntTable table;
  BOOST_TEST((table.begin() + 0 == table.end()));
  BOOST_TEST(table.end() - table.begin() == 0);

  table.push_back(42);
  BOOST_TEST((table.begin() + 1 == table.end()));
  BOOST_TEST((table.end() - 1 == table.begin()));
  BOOST_TEST(table.begin()[0] == 42);
}

BOOST_AUTO_TEST_CASE(StdSort) {
  std::vector<int> reference;
  auto table = make_varied_table(reference);
  std::mt19937 rng(11);
  std::shuffle(table.begin(), table.end(), rng);
  std::sort(table.begin(), table.end());
  std::sort(reference.begin(), reference.end());
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(StdBoundsMatchCustomBounds) {
  std::vector<int> reference;
  auto table = make_varied_table(reference);
  std::sort(table.begin(), table.end());
  std::sort(reference.begin(), reference.end());

  std::mt19937 rng(13);
  std::uniform_int_distribution<int> value_dist(
      reference.front() - 2,
      reference.back() + 2);
  for(int i = 0; i < 2000; ++i) {
    int value = value_dist(rng);
    auto expected_lower = std::lower_bound(reference.begin(), reference.end(), value) -
                          reference.begin();
    auto expected_upper = std::upper_bound(reference.begin(), reference.end(), value) -
                          reference.begin();
    BOOST_TEST_INFO("value=" << value);
    BOOST_TEST(
        std::lower_bound(table.begin(), table.end(), value) - table.begin() ==
        expected_lower);
    BOOST_TEST(
        std::upper_bound(table.begin(), table.end(), value) - table.begin() ==
        expected_upper);
    BOOST_TEST(
        table.lower_bound(table.begin(), table.end(), value, std::less<int>()) -
            table.begin() ==
        expected_lower);
    BOOST_TEST(
        table.upper_bound(table.begin(), table.end(), value, std::less<int>()) -
            table.begin() ==
        expected_upper);
  }
}

BOOST_AUTO_TEST_CASE(StdInplaceMerge) {
  std::vector<int> reference;
  auto table = make_varied_table(reference);
  auto middle = static_cast<std::ptrdiff_t>(reference.size() / 3);
  std::sort(table.begin(), table.begin() + middle);
  std::sort(table.begin() + middle, table.end());
  std::inplace_merge(table.begin(), table.begin() + middle, table.end());
  std::sort(reference.begin(), reference.end());
  check_contents(table, reference);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_BoundsWithDuplicates)

namespace {

// Sorted values with long runs of equal values. Runs of 1000 cross segment
// boundaries, and one run is long enough to span several whole segments.
std::vector<int> make_runs_reference() {
  std::vector<int> reference;
  for(int value = 0; value < 40; ++value) {
    int run = value == 7 ? kSplit * 2 + kSplit / 2 : 1000;
    reference.insert(reference.end(), run, value * 2);
  }
  return reference;
}

// Compares every bound against the standard algorithms on the reference, for
// the subrange [first, last) of the table. Returns false after the first
// mismatch so that a bug doesn't produce thousands of errors.
bool check_bounds_on_subrange(
    IntTable& table,
    std::vector<int> const& reference,
    std::ptrdiff_t first,
    std::ptrdiff_t last,
    int value) {
  auto const ref_first = reference.begin() + first;
  auto const ref_last = reference.begin() + last;
  auto const expected_lower = std::lower_bound(ref_first, ref_last, value) -
                              reference.begin();
  auto const expected_upper = std::upper_bound(ref_first, ref_last, value) -
                              reference.begin();

  auto const table_first = table.begin() + first;
  auto const table_last = table.begin() + last;
  IntTable const& const_table = table;
  std::ptrdiff_t const results[] = {
      table.lower_bound(table_first, table_last, value, std::less<int>()) -
          table.begin(),
      const_table.lower_bound(table_first, table_last, value, std::less<int>()) -
          const_table.begin(),
      std::lower_bound(table_first, table_last, value) - table.begin(),
      table.upper_bound(table_first, table_last, value, std::less<int>()) -
          table.begin(),
      const_table.upper_bound(table_first, table_last, value, std::less<int>()) -
          const_table.begin(),
      std::upper_bound(table_first, table_last, value) - table.begin()};
  char const* const names[] = {
      "lower_bound",
      "const lower_bound",
      "std::lower_bound",
      "upper_bound",
      "const upper_bound",
      "std::upper_bound"};

  for(int i = 0; i < 6; ++i) {
    auto const expected = i < 3 ? expected_lower : expected_upper;
    if(results[i] != expected) {
      BOOST_ERROR(
          names[i] << " of " << value << " in [" << first << ", " << last
                   << ") returned " << results[i] << ", expected " << expected);
      return false;
    }
  }
  return true;
}

std::vector<int> values_to_search(std::vector<int> const& reference) {
  // Every run value, values between runs, and values outside all of them.
  std::vector<int> values;
  for(int v = reference.front() - 1; v <= reference.back() + 1; ++v) {
    values.push_back(v);
  }
  return values;
}

} // namespace

BOOST_AUTO_TEST_CASE(FullRangeAndSegmentBoundaries) {
  auto reference = make_runs_reference();
  IntTable table(reference.begin(), reference.end());
  auto const size = static_cast<std::ptrdiff_t>(reference.size());

  // Built with push_back, so segments are exactly kSplit long. Use subranges
  // that start and end on, just before and just after segment boundaries.
  std::vector<std::ptrdiff_t> ends = {0, 1, size - 1, size};
  for(std::ptrdiff_t boundary = kSplit; boundary < size; boundary += kSplit) {
    ends.insert(ends.end(), {boundary - 1, boundary, boundary + 1});
  }

  for(int value : values_to_search(reference)) {
    for(auto first : ends) {
      for(auto last : ends) {
        if(first <= last &&
           !check_bounds_on_subrange(table, reference, first, last, value)) {
          return;
        }
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(RandomSubrangesAfterSplits) {
  auto reference = make_runs_reference();
  IntTable table(reference.begin(), reference.end());
  std::mt19937 rng(17);

  auto check_random_subranges = [&](int count) {
    auto const size = static_cast<std::ptrdiff_t>(reference.size());
    auto const values = values_to_search(reference);
    for(int i = 0; i < count; ++i) {
      auto a = std::uniform_int_distribution<std::ptrdiff_t>(0, size)(rng);
      auto b = std::uniform_int_distribution<std::ptrdiff_t>(0, size)(rng);
      int value = values[rng() % values.size()];
      if(!check_bounds_on_subrange(table, reference, std::min(a, b), std::max(a, b), value)) {
        return false;
      }
    }
    return true;
  };

  BOOST_REQUIRE(check_random_subranges(3000));

  // Insert duplicates at their upper bounds so that segments split at
  // positions that don't line up with the runs.
  for(int i = 0; i < 20000; ++i) {
    int value = 2 * static_cast<int>(rng() % 40);
    auto it = table.upper_bound(table.begin(), table.end(), value, std::less<int>());
    table.insert(it, value);
    reference.insert(
        std::upper_bound(reference.begin(), reference.end(), value),
        value);
  }
  check_contents(table, reference);
  check_random_subranges(3000);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_SegmentSizes)

namespace {

// Every segment other than the last holds between kMergeThreshold and
// kSplitThreshold elements, unless there is only one segment. No segment has
// room for more than kSplitThreshold elements. Other than the last segment,
// which reserves a whole segment when push_back starts it, no segment has
// room for much more than twice what it holds.
void check_segment_sizes(IntTable const& table, char const* phase) {
  auto sizes = table.segment_sizes();
  auto capacities = table.segment_capacities();
  BOOST_TEST(capacities.size() == sizes.size());
  for(std::size_t i = 0; i < sizes.size() && i < capacities.size(); ++i) {
    BOOST_TEST_INFO(
        "phase: " << phase << ", segment " << i << " of " << sizes.size());
    BOOST_TEST(capacities[i] <= static_cast<std::size_t>(kSplit));
    if(i + 1 < sizes.size()) {
      BOOST_TEST_INFO(
          "phase: " << phase << ", segment " << i << " of " << sizes.size());
      BOOST_TEST((capacities[i] <= 2 * sizes[i] || capacities[i] <= 64u));
    }
  }
  BOOST_TEST_INFO("phase: " << phase);
  BOOST_TEST(!sizes.empty());
  for(std::size_t i = 0; i < sizes.size(); ++i) {
    BOOST_TEST_INFO(
        "phase: " << phase << ", segment " << i << " of " << sizes.size());
    BOOST_TEST(sizes[i] <= static_cast<std::size_t>(kSplit));
    if(sizes.size() > 1) {
      BOOST_TEST(sizes[i] > 0u);
      if(i + 1 < sizes.size()) {
        BOOST_TEST(sizes[i] >= static_cast<std::size_t>(kMerge));
      }
    }
  }
}

} // namespace

BOOST_AUTO_TEST_CASE(PushBackAndPopBack) {
  // Segments filled by push_back grow gradually rather than reserving a
  // whole segment, and pop_back gives memory back.
  IntTable table;
  std::vector<int> reference;
  for(int i = 0; i < kSplit * 3 + 100; ++i) {
    table.push_back(i);
    reference.push_back(i);
    if(i % 997 == 0) {
      check_segment_sizes(table, "push_back");
    }
  }
  check_segment_sizes(table, "pushed");
  while(reference.size() > 5) {
    table.pop_back();
    reference.pop_back();
    if(reference.size() % 997 == 0) {
      check_segment_sizes(table, "pop_back");
    }
  }
  check_segment_sizes(table, "popped");
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(SizesStayWithinThresholds) {
  int const count = kSplit * 8;
  auto table = make_sequence(count);
  auto reference = make_reference(count);
  check_segment_sizes(table, "built");
  std::mt19937 rng(29);

  // Random erases shrink segments well below the merge threshold.
  for(int i = 0; i < count / 2; ++i) {
    auto index = static_cast<std::ptrdiff_t>(rng() % reference.size());
    table.erase(table.begin() + index);
    reference.erase(reference.begin() + index);
  }
  check_segment_sizes(table, "erased");
  check_contents(table, reference);

  // Random inserts split segments.
  for(int i = 0; i < count; ++i) {
    auto index = static_cast<std::ptrdiff_t>(rng() % (reference.size() + 1));
    table.insert(table.begin() + index, -i);
    reference.insert(reference.begin() + index, -i);
  }
  check_segment_sizes(table, "inserted");
  check_contents(table, reference);

  // Rotating small blocks splits segments either side of each cut.
  auto const size = static_cast<int>(reference.size());
  for(int i = 0; i < 500; ++i) {
    int block = 1 + static_cast<int>(rng() % 50);
    int first = static_cast<int>(rng() % (size - block));
    int target = static_cast<int>(rng() % (size + 1));
    if(target >= first && target <= first + block) {
      continue;
    }
    IntTable::iterator result;
    std::vector<int>::iterator ref_result;
    if(target > first) {
      result = table.rotate(
          table.begin() + first,
          table.begin() + first + block,
          table.begin() + target);
      ref_result = std::rotate(
          reference.begin() + first,
          reference.begin() + first + block,
          reference.begin() + target);
    }
    else {
      result = table.rotate(
          table.begin() + target,
          table.begin() + first,
          table.begin() + first + block);
      ref_result = std::rotate(
          reference.begin() + target,
          reference.begin() + first,
          reference.begin() + first + block);
    }
    if(result - table.begin() != ref_result - reference.begin()) {
      BOOST_ERROR("rotate returned the wrong position at rotation " << i);
      return;
    }
  }
  check_segment_sizes(table, "rotated");
  check_contents(table, reference);

  // Erase almost everything.
  while(reference.size() > 10) {
    auto index = static_cast<std::ptrdiff_t>(rng() % reference.size());
    auto it = table.erase(table.begin() + index);
    reference.erase(reference.begin() + index);
    if(it - table.begin() != index) {
      BOOST_ERROR("erase returned the wrong position");
      return;
    }
  }
  check_segment_sizes(table, "emptied");
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(EraseRanges) {
  int const count = kSplit * 4 + 7;
  std::vector<std::pair<int, int>> const ranges = {
      {0, 0},
      {5, 5},
      {0, count},
      {0, 10},
      {10, 20},
      {kSplit - 5, kSplit + 5},
      {kSplit, kSplit * 2},
      {5, kSplit * 3 + 5},
      {kSplit / 2, count},
      {count - 1, count},
      {1, count - 1},
      {kMerge, kSplit * 2 - kMerge}};
  for(auto [first, last] : ranges) {
    auto table = make_sequence(count);
    auto reference = make_reference(count);
    auto it = table.erase(table.cbegin() + first, table.cbegin() + last);
    reference.erase(reference.begin() + first, reference.begin() + last);
    BOOST_TEST_INFO("first=" << first << " last=" << last);
    BOOST_TEST(it - table.begin() == first);
    check_contents(table, reference);
    check_segment_sizes(table, "erase range");
  }
}

BOOST_AUTO_TEST_CASE(InsertRanges) {
  int const count = kSplit * 2 + 7;
  std::vector<int> const positions = {0, 5, kSplit - 1, kSplit, kSplit + 5, count};
  std::vector<int> const counts =
      {0, 1, 10, kSplit - 1, kSplit, kSplit + 1, kSplit * 3 + 5};
  for(int pos : positions) {
    for(int n : counts) {
      std::vector<int> values(static_cast<std::size_t>(n));
      std::iota(values.begin(), values.end(), -n);
      auto table = make_sequence(count);
      auto reference = make_reference(count);
      auto it = table.insert(table.cbegin() + pos, values.begin(), values.end());
      reference.insert(reference.begin() + pos, values.begin(), values.end());
      BOOST_TEST_INFO("pos=" << pos << " count=" << n);
      BOOST_TEST(it - table.begin() == pos);
      check_contents(table, reference);
      check_segment_sizes(table, "insert range");
    }
  }
}

BOOST_AUTO_TEST_CASE(InsertInputIteratorRange) {
  std::istringstream in("-1 -2 -3");
  auto table = make_sequence(kSplit + 1);
  auto reference = make_reference(kSplit + 1);
  auto it = table.insert(
      table.cbegin() + 5,
      std::istream_iterator<int>(in),
      std::istream_iterator<int>());
  reference.insert(reference.begin() + 5, {-1, -2, -3});
  BOOST_TEST(it - table.begin() == 5);
  check_contents(table, reference);
}

BOOST_AUTO_TEST_CASE(FillAndInitializerList) {
  IntTable filled(kSplit * 2 + 3, 7);
  check_contents(filled, std::vector<int>(kSplit * 2 + 3, 7));
  check_segment_sizes(filled, "filled");

  filled.assign(5, 1);
  check_contents(filled, {1, 1, 1, 1, 1});

  auto it = filled.insert(filled.cbegin() + 2, kSplit, 2);
  BOOST_TEST(it - filled.begin() == 2);
  std::vector<int> reference = {1, 1, 1, 1, 1};
  reference.insert(reference.begin() + 2, kSplit, 2);
  check_contents(filled, reference);

  it = filled.insert(filled.cbegin() + 1, {3, 4});
  BOOST_TEST(it - filled.begin() == 1);
  reference.insert(reference.begin() + 1, {3, 4});
  check_contents(filled, reference);

  IntTable list = {1, 2, 3};
  check_contents(list, {1, 2, 3});
  list = {4, 5};
  check_contents(list, {4, 5});
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SegmentedSequence_SmallSegments)

namespace {

// Tiny segments mean many segments and constant splitting, merging and
// rebalancing, so the same code paths as with the default thresholds run far
// more often, and non-default thresholds are covered.
using SmallTable = daily::SegmentedSequence<int, 64, 16>;

// Searching is only checked if the contents are sorted.
bool check_small_table(
    SmallTable const& table,
    std::vector<int> const& reference,
    std::mt19937& rng,
    bool sorted = true) {
  if(table.size() != reference.size() ||
     !std::equal(table.begin(), table.end(), reference.begin(), reference.end()) ||
     !std::equal(table.rbegin(), table.rend(), reference.rbegin(), reference.rend())) {
    BOOST_ERROR("Contents differ from the reference");
    return false;
  }
  for(std::size_t i = 0; i < reference.size(); ++i) {
    if(table.begin()[static_cast<std::ptrdiff_t>(i)] != reference[i]) {
      BOOST_ERROR("Random access differs from the reference at " << i);
      return false;
    }
  }

  auto sizes = table.segment_sizes();
  auto capacities = table.segment_capacities();
  for(std::size_t i = 0; i < sizes.size(); ++i) {
    bool const last = i + 1 == sizes.size();
    bool ok = sizes[i] <= 64 && capacities[i] <= 64;
    if(sizes.size() > 1) {
      ok = ok && sizes[i] > 0 && (last || sizes[i] >= 16);
    }
    // The last segment reserves a whole segment when push_back starts it.
    ok = ok && (last || capacities[i] <= 2 * sizes[i] + 16);
    if(!ok) {
      BOOST_ERROR(
          "Segment " << i << " of " << sizes.size() << " has size " << sizes[i]
                     << " and capacity " << capacities[i]);
      return false;
    }
  }

  auto const size = static_cast<std::ptrdiff_t>(reference.size());
  for(int i = 0; sorted && i < 50; ++i) {
    auto a = std::uniform_int_distribution<std::ptrdiff_t>(0, size)(rng);
    auto b = std::uniform_int_distribution<std::ptrdiff_t>(0, size)(rng);
    auto first = std::min(a, b);
    auto last = std::max(a, b);
    int value = static_cast<int>(rng() % 1100) - 50;
    auto expected_lower = std::lower_bound(
                              reference.begin() + first,
                              reference.begin() + last,
                              value) -
                          reference.begin();
    auto expected_upper = std::upper_bound(
                              reference.begin() + first,
                              reference.begin() + last,
                              value) -
                          reference.begin();
    auto lower = table.lower_bound(
                     table.begin() + first,
                     table.begin() + last,
                     value,
                     std::less<int>()) -
                 table.begin();
    auto upper = table.upper_bound(
                     table.begin() + first,
                     table.begin() + last,
                     value,
                     std::less<int>()) -
                 table.begin();
    if(lower != expected_lower || upper != expected_upper) {
      BOOST_ERROR(
          "Bounds of " << value << " in [" << first << ", " << last << ") are "
                       << lower << ", " << upper << ", expected "
                       << expected_lower << ", " << expected_upper);
      return false;
    }
  }
  return true;
}

} // namespace

BOOST_AUTO_TEST_CASE(RandomOperationsMatchReference) {
  SmallTable table;
  std::vector<int> reference;
  std::mt19937 rng(37);

  for(int op = 0; op < 200000; ++op) {
    // Alternate between growing and shrinking phases.
    bool const growing = (op / 25000) % 2 == 0;
    int const choice = static_cast<int>(rng() % 100);
    auto const size = static_cast<std::ptrdiff_t>(reference.size());

    if(choice < (growing ? 55 : 35) || size < 4) {
      int value = static_cast<int>(rng() % 1000);
      auto it = table.upper_bound(table.begin(), table.end(), value, std::less<int>());
      auto inserted = table.insert(it, value);
      auto ref_it = reference.insert(
          std::upper_bound(reference.begin(), reference.end(), value),
          value);
      if(inserted - table.begin() != ref_it - reference.begin()) {
        BOOST_ERROR("insert returned the wrong position at op " << op);
        return;
      }
    }
    else if(choice < 90) {
      auto index = static_cast<std::ptrdiff_t>(
          rng() % static_cast<std::size_t>(size));
      auto next = table.erase(table.begin() + index);
      reference.erase(reference.begin() + index);
      if(next - table.begin() != index) {
        BOOST_ERROR("erase returned the wrong position at op " << op);
        return;
      }
    }
    else if(choice < 96) {
      // Move a block away and back, which keeps the order.
      auto block = 1 + static_cast<std::ptrdiff_t>(
                           rng() % static_cast<std::size_t>(
                                       std::min<std::ptrdiff_t>(size - 1, 200)));
      auto first = static_cast<std::ptrdiff_t>(
          rng() % static_cast<std::size_t>(size - block + 1));
      auto target = static_cast<std::ptrdiff_t>(
          rng() % static_cast<std::size_t>(size + 1));
      if(target > first + block) {
        table.rotate(
            table.begin() + first,
            table.begin() + first + block,
            table.begin() + target);
        table.rotate(
            table.begin() + first,
            table.begin() + target - block,
            table.begin() + target);
      }
      else if(target < first) {
        table.rotate(
            table.begin() + target,
            table.begin() + first,
            table.begin() + first + block);
        table.rotate(
            table.begin() + target,
            table.begin() + target + block,
            table.begin() + first + block);
      }
    }
    else if(choice < 98) {
      int value = reference.empty()
                      ? 0
                      : reference.back() + static_cast<int>(rng() % 3);
      table.push_back(value);
      reference.push_back(value);
    }
    else {
      table.pop_back();
      reference.pop_back();
    }

    if(op % 1000 == 0 && !check_small_table(table, reference, rng)) {
      BOOST_ERROR("after op " << op);
      return;
    }
  }
  check_small_table(table, reference, rng);
}

BOOST_AUTO_TEST_CASE(RandomRangeOperationsMatchReference) {
  SmallTable table;
  std::vector<int> reference;
  std::mt19937 rng(41);

  for(int op = 0; op < 20000; ++op) {
    // Alternate between growing and shrinking phases.
    bool const growing = (op / 2000) % 2 == 0;
    auto const size = static_cast<std::ptrdiff_t>(reference.size());
    auto const pos = std::uniform_int_distribution<std::ptrdiff_t>(0, size)(rng);
    // Mostly short ranges, sometimes ones spanning several segments.
    auto const max_count = rng() % 4 == 0 ? 300 : 20;
    auto const count = std::uniform_int_distribution<std::ptrdiff_t>(0, max_count)(
        rng);

    std::ptrdiff_t result = 0;
    if(rng() % 3 != (growing ? 0u : 1u) || size == 0) {
      std::vector<int> values(static_cast<std::size_t>(count));
      std::iota(values.begin(), values.end(), op * 1000);
      auto const at = table.cbegin() + pos;
      switch(rng() % 3) {
        case 0:
          result = table.insert(at, values.begin(), values.end()) - table.begin();
          break;
        case 1: {
          std::list<int> list(values.begin(), values.end());
          result = table.insert(at, list.begin(), list.end()) - table.begin();
          break;
        }
        default: {
          std::ostringstream out;
          for(int v : values) {
            out << v << ' ';
          }
          std::istringstream in(out.str());
          result = table.insert(
                       at,
                       std::istream_iterator<int>(in),
                       std::istream_iterator<int>()) -
                   table.begin();
          break;
        }
      }
      reference.insert(reference.begin() + pos, values.begin(), values.end());
    }
    else {
      auto const last = std::min(size, pos + count);
      result = table.erase(table.cbegin() + pos, table.cbegin() + last) -
               table.begin();
      reference.erase(reference.begin() + pos, reference.begin() + last);
    }

    if(result != pos) {
      BOOST_ERROR("Range operation returned the wrong position at op " << op);
      return;
    }
    if(op % 10 == 0 && !check_small_table(table, reference, rng, false)) {
      BOOST_ERROR("after op " << op);
      return;
    }
  }
  check_small_table(table, reference, rng, false);
}

BOOST_AUTO_TEST_SUITE_END()
