#include "TestEnvironment.hpp"
#include <cstdio>
#include <gtest/gtest.h>
#include "MultiThread/functions.hpp"
#include "log_engine.hpp"
#include "md5.hpp"
#include "string_generator.hpp"

TEST(TestMt, test_chunk_function){
    auto chunk = std::vector<std::string>{"hello","world","!"};
    auto target = MD5(chunk[1]).hexdigest();
    auto result = compute_chunk(chunk, target, 3);
    ASSERT_EQ(result, chunk[1]);
}


TEST(TestMt, test_flat_function){
    AssignedSequenceGenerator gen{4};
    auto chunk = gen.generate_chunk(1000);
    size_t size = 0;
    #pragma omp parallel for reduction(+:size)
    for(auto i = 0; i < chunk.size(); i++){
        size += chunk[i].size();
    }
    ASSERT_EQ(size, 4000);
    char buffer[size];
    memset(buffer, 0, size);
    auto res = flatten_chunk(chunk, buffer);
    auto disp = res.disp;
    for(int i = 0 ; i < disp.size(); i++){
        char str[5]{};
        memcpy(str, &buffer[disp[i]], res.sizes[i]);
        ASSERT_STREQ(chunk[i].c_str(), str);
    }
    
}