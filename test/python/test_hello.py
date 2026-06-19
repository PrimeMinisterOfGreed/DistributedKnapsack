import sys
from pathlib import Path

BUILD_DIR = Path(__file__).parent.parent.parent / "build"
sys.path.insert(0, str(BUILD_DIR))

import libdistributed_knapsack as lib

def test_hello():
    lib.hello()
    assert True

def test_knapsackdp():
    weights = [1, 2, 3]
    values = [10, 20, 30]
    capacity = 4
    result = lib.knapsackdp(weights, values, capacity, 1)
    print(f"Knapsack result: {result.totalValue}, Total Weight: {result.totalWeight}")

if __name__ == "__main__":
    test_hello()
    test_knapsackdp()
    print("Python test passed!")
