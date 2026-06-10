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
    data.resize(a.size() + b.size());
    parallel_merge(a, b, data, 2);



    std::vector<int> expected{0,1,2,3,5,6,7,11,12};
    EXPECT_EQ(data, expected);
}

TEST(DivideInBalancedBlocks, SingleThreadDividesCorrectly)
{
    std::vector<CopaSubset> input{
        CopaSubset{{1}, 1, 10},
        CopaSubset{{2}, 2, 5},
        CopaSubset{{3}, 3, 20},
        CopaSubset{{4}, 4, 15}
    };

    std::vector<CopaBlock> blocks(1);
    distribute_block_per_processor(input, blocks, 1);

    ASSERT_EQ(blocks.size(), 1u);
    EXPECT_EQ(blocks[0].block.size(), 4u);
    EXPECT_EQ(blocks[0].maxValue, 20);
}

TEST(DivideInBalancedBlocks, MultiThreadDividesCorrectly)
{
    std::vector<CopaSubset> input{
        CopaSubset{{1}, 1, 10},
        CopaSubset{{2}, 2, 5},
        CopaSubset{{3}, 3, 20},
        CopaSubset{{4}, 4, 15}
    };

    std::vector<CopaBlock> blocks(2);
    distribute_block_per_processor(input, blocks, 2);

    ASSERT_EQ(blocks.size(), 2u);
    EXPECT_EQ(blocks[0].block.size(), 2u);
    EXPECT_EQ(blocks[1].block.size(), 2u);
    EXPECT_EQ(blocks[0].maxValue, 10);
    EXPECT_EQ(blocks[1].maxValue, 20);
}

TEST(DivideInBalancedBlocks, EmptyInputDoesNotModifyOutput)
{
    std::vector<CopaSubset> input;
    std::vector<CopaBlock> blocks(2);
    blocks[0] = {std::span<const CopaSubset>{}, 42};
    blocks[1] = {std::span<const CopaSubset>{}, 99};
    distribute_block_per_processor(input, blocks, 2);
    EXPECT_EQ(blocks[0].maxValue, 42);
    EXPECT_EQ(blocks[1].maxValue, 99);
}

TEST(DivideInBalancedBlocks, UnevenDivisionHandlesRemainder)
{
    std::vector<CopaSubset> input{
        CopaSubset{{1}, 1, 10},
        CopaSubset{{2}, 2, 20},
        CopaSubset{{3}, 3, 30},
        CopaSubset{{4}, 4, 40},
        CopaSubset{{5}, 5, 50}
    };

    std::vector<CopaBlock> blocks(2);
    distribute_block_per_processor(input, blocks, 2);

    ASSERT_EQ(blocks.size(), 2u);
    EXPECT_EQ(blocks[0].block.size(), 3u);
    EXPECT_EQ(blocks[1].block.size(), 2u);
    EXPECT_EQ(blocks[0].maxValue, 30);
    EXPECT_EQ(blocks[1].maxValue, 50);
}

TEST(DivideInBalancedBlocks, BlocksReferenceOriginalData)
{
    std::vector<CopaSubset> input{
        CopaSubset{{1}, 1, 10},
        CopaSubset{{2}, 2, 20},
        CopaSubset{{3}, 3, 30}
    };

    std::vector<CopaBlock> blocks(3);
    distribute_block_per_processor(input, blocks, 3);

    ASSERT_EQ(blocks.size(), 3u);
    EXPECT_EQ(&blocks[0].block.front(), &input[0]);
    EXPECT_EQ(&blocks[1].block.front(), &input[1]);
    EXPECT_EQ(&blocks[2].block.front(), &input[2]);
}

