#include "knapsack.hpp"



struct IntermediateResult {
    std::vector<std::vector<int>> dp; // Partial DP table for the assigned items
    int startIndex; // Starting index of the items assigned to this process
    int endIndex;   // Ending index of the items assigned to this process
};



KnapsackSolution knapsackdpmpi(boost::mpi::communicator& comm, const std::vector<int>& weights, const std::vector<int>& values, int capacity)
{
        
}