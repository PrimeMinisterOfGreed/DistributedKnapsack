import random
import sys
from pathlib import Path
from typing import Tuple
BUILD_DIR = Path(__file__).parent.parent / "build"
sys.path.insert(0, str(BUILD_DIR))

import libdistributed_knapsack as lib
import argparse

from testengine import (
    TestRegister, BenchmarkKnapsackDP, BenchmarkKnapsackCOPA,
    BenchmarkKnapsackCOPASerial, BenchmarkKnapsackCOPAMPI, BenchmarkKnapsackDPMPI
)

def generate_data(num_items: int, min_weight: int, max_weight: int,
                  min_value: int, max_value: int, seed: int) -> Tuple[list[int], list[int]]:
    random.seed(seed)
    weights = [random.randint(min_weight, max_weight) for _ in range(num_items)]
    values = [random.randint(min_value, max_value) for _ in range(num_items)]
    return weights, values


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description="Distributed Knapsack Benchmark")
    parser.add_argument("--test", type=str, default="all", 
                        help="Test to run: knapsackdp, knapsackcopa, knapsackcopa_serial, "
                             "knapsackcopa_mpi, knapsackdp_mpi, all")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--numItems", type=int, default=30, help="Number of items")
    parser.add_argument("--numThreads", type=int, default=1, help="Number of threads")
    parser.add_argument("--capacity", type=int, default=1000, help="Knapsack capacity")
    parser.add_argument("--minWeight", type=int, default=1, help="Minimum weight")
    parser.add_argument("--maxWeight", type=int, default=50, help="Maximum weight")
    parser.add_argument("--minValue", type=int, default=1, help="Minimum value")
    parser.add_argument("--maxValue", type=int, default=50, help="Maximum value")
    parser.add_argument("--save", type=str, default="results.csv", 
                        help="Output CSV file for results (default: results.csv)")
    
    args = parser.parse_args()
    
    weights, values = generate_data(
        args.numItems, args.minWeight, args.maxWeight,
        args.minValue, args.maxValue, args.seed
    )
    
    register = TestRegister(save_file=args.save, capacity=args.capacity)
    register.register("knapsackdp", BenchmarkKnapsackDP())
    register.register("knapsackcopa", BenchmarkKnapsackCOPA())
    register.register("knapsackcopa_serial", BenchmarkKnapsackCOPASerial())
    register.register("knapsackcopa_mpi", BenchmarkKnapsackCOPAMPI())
    register.register("knapsackdp_mpi", BenchmarkKnapsackDPMPI())
    
    register.setup(weights, values, args.numThreads)
    register.run(args.test)
