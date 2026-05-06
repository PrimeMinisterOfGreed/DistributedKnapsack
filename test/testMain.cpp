#include <gtest/gtest.h>
#include "options_bag.hpp"


ProgramOptions options{};

void fillOptionsMap()
{

}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

