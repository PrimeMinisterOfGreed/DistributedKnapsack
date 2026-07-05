import sys
from pathlib import Path
import random
BUILD_DIR = Path(__file__).parent.parent.parent / "build"
sys.path.insert(0, str(BUILD_DIR))

import libdistributed_knapsack as lib

def test_hello():
    lib.hello()
    assert True

def test_knapsackdp():
    random.seed(42)
    weights = [random.randint(1, 50) for x in range(30)]
    values = [random.randint(1, 50) for x in range(30)]
    capacity = random.randint(1, 1000)
    args = lib.KnapsackArguments(weights, values, capacity)
    result = lib.knapsackdp(args,1)
    print(f"Knapsack result: {result.totalValue}, Total Weight: {result.totalWeight}")
    #result_copa = lib.knapsackcopa(args,16)
   # print(f"Knapsack COPA result: {result_copa.totalValue}, Total Weight: {result_copa.totalWeight}")
   #result_copa_seq = lib.knapsackcopasequential(args)
    #print(f"Knapsack COPA Sequential result: {result_copa_seq.totalValue}, Total Weight: {result_copa_seq.totalWeight}")
    result_mpi = lib.knapsackcopampi(args)
    print(f"Knapsack MPI result: {result_mpi.totalValue}, Total Weight: {result_mpi.totalWeight}")

if __name__ == "__main__":
    test_hello()
    test_knapsackdp()
    print("Python test passed!")
