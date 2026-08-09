import sys
import time
import random
import argparse
import csv
import os
from abc import ABC, abstractmethod
from typing import Dict, Tuple, Optional
from pathlib import Path
from mpi4py import MPI  # type: ignore
from libdistributed_knapsack import KnapsackArguments, KnapsackSolution, knapsackdp, knapsackcopa, knapsackcopasequential, knapsackcopampi, knapsackdpmpi


class BenchmarkTest(ABC):
    def __init__(self) -> None:
        self.args: KnapsackArguments
        self.numThreads: int = 1
        self.numItems: int = 0
        self.is_mpi_test: bool = False

    @abstractmethod
    def onExecute(self) -> KnapsackSolution:
        pass

    def setup(self, weights: list[int], values: list[int], 
              capacity: int, numThreads: int = 1) -> None:
        self.args =    KnapsackArguments(weights, values, capacity)
        self.numThreads = numThreads
        self.numItems = len(weights)

    def execute(self) -> Tuple[float, KnapsackSolution]:
        start_time = time.time()
        result = self.onExecute()
        end_time = time.time()
        return end_time - start_time, result


def check_mpi_initialized() -> None:
    if not MPI.Is_initialized():
        raise RuntimeError("MPI not initialized. Run with mpirun for MPI tests.")


class BenchmarkKnapsackDP(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackdp(self.args, self.numThreads)


class BenchmarkKnapsackCOPA(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackcopa(self.args, self.numThreads)


class BenchmarkKnapsackCOPASerial(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackcopasequential(self.args)


class BenchmarkKnapsackCOPAMPI(BenchmarkTest):
    def __init__(self) -> None:
        super().__init__()
        self.is_mpi_test = True

    def onExecute(self) -> KnapsackSolution:
        check_mpi_initialized()
        return knapsackcopampi(self.args)


class BenchmarkKnapsackDPMPI(BenchmarkTest):
    def __init__(self) -> None:
        super().__init__()
        self.is_mpi_test = True

    def onExecute(self) -> KnapsackSolution:
        check_mpi_initialized()
        return knapsackdpmpi(self.args)


class TestRegister:
    def __init__(self, save_file: Optional[str] = None, capacity: int = 0) -> None:
        self._tests: Dict[str, BenchmarkTest] = {}
        self._save_file = save_file
        self._capacity = capacity

    def register(self, name: str, test: BenchmarkTest) -> None:
        self._tests[name] = test

    def setup(self, weights: list[int], values: list[int], numThreads: int = 1) -> None:
        """Setup all registered tests with the same data."""
        for test in self._tests.values():
            test.setup(weights, values, self._capacity, numThreads)

    def _append_result(self, test_name: str, test: BenchmarkTest, 
                       duration: float, result: KnapsackSolution) -> None:
        """Append test result to CSV file."""
        if not self._save_file:
            return
            
        file_exists = os.path.exists(self._save_file)
        
        processors = MPI.COMM_WORLD.size if test.is_mpi_test else test.numThreads
        test_type = "distributed memory" if test.is_mpi_test else "shared memory"
        
        with open(self._save_file, mode='a', newline='') as f:
            writer = csv.writer(f)
            hostname = MPI.Get_processor_name() if test.is_mpi_test else os.uname().nodename
            if not file_exists:
                writer.writerow(['hostname','testname', 'testtype', 'time', 'processors', 'solution_weight', 'solution_profit', 'capacity', 'num_items'])
            writer.writerow([hostname, test_name, test_type, f"{duration:.4f}", processors, result.totalWeight, result.totalValue, self._capacity, test.numItems])

    def run(self, name: str = "all") -> None:
        if name == "all":
            tests_to_run = self._tests
        else:
            if name not in self._tests:
                raise ValueError(f"Test '{name}' not found")
            tests_to_run = {name: self._tests[name]}
        
        for test_name, test in tests_to_run.items():
            duration, result = test.execute()
            if test.is_mpi_test:
                if MPI.COMM_WORLD.rank != 0:
                    continue  # Only rank 0 prints results for MPI tests
            print(f"{test_name}: {duration:.4f}s | Items: {test.numItems} | "
                  f"Profit: {result.totalValue} | Weight: {result.totalWeight}")
            
            if self._save_file:
                self._append_result(test_name, test, duration, result)



