#include "Knapsack/utils.tpp"
#include "TestEnvironment.hpp"
#include <gtest/gtest.h>
#include <vector>
#include "Knapsack/utils.hpp"
#include <algorithm>

TEST(ParallelMerge, MergesAdjacentSortedRangesCorrectlySingleThread)
{
    std::vector<int> a{1, 4, 9};
    std::vector<int> b{2, 3, 8, 10};

    // put both ranges into a single contiguous container as expected by parallel_merge
    std::vector<int> data;
    data.reserve(a.size() + b.size() + 1);
    parallel_merge(a, b, data, 1);

    std::vector<int> expected{1,2,3,4,8,9,10};
    EXPECT_EQ(data, expected);
}

TEST(ParallelMerge, MergesAdjacentSortedRangesCorrectlyMultiThread)
{
    std::vector<int> a{0, 5, 6, 11};
    std::vector<int> b{1, 2, 3, 7, 12};

    std::vector<int> data;
    data.reserve(a.size() + b.size() + 1);
    parallel_merge(a, b, data, 2);



    std::vector<int> expected{0,1,2,3,5,6,7,11,12};
    EXPECT_EQ(data, expected);
}

