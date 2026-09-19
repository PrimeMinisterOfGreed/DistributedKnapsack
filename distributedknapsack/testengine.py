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
from libdistributed_knapsack import (KnapsackArguments, KnapsackSolution, knapsackdp, knapsackcopa,
                                     knapsackcopasequential, knapsackdpdag, knapsackdpmpi, knapsackdpgpu,
                                     get_all_sections, get_dag_stats)


def ensure_mpi_initialized() -> None:
    if not MPI.Is_initialized():
        MPI.Init()


class BenchmarkTest(ABC):
    def __init__(self) -> None:
        self.args: KnapsackArguments
        self.numThreads: int = 1
        self.numItems: int = 0
        self.testType :str = ""
        self.is_mpi_test: bool = False
        self.is_dag_test: bool = False
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



class BenchmarkKnapsackDP(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackdp(self.args)


class BenchmarkKnapsackCOPA(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackcopa(self.args)


class BenchmarkKnapsackCOPASerial(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackcopasequential(self.args)


class BenchmarkKnapsackDPDAG(BenchmarkTest):
    def __init__(self, item_block: int = 10, cap_block: int = 0) -> None:
        super().__init__()
        self.item_block = item_block
        self.cap_block = cap_block
        self.is_dag_test = True

    def onExecute(self) -> KnapsackSolution:
        return knapsackdpdag(self.args, self.item_block, self.cap_block)


class BenchmarkKnapsackDPMPI(BenchmarkTest):
    def __init__(self) -> None:
        super().__init__()
        self.is_mpi_test = True

    def onExecute(self) -> KnapsackSolution:
        ensure_mpi_initialized()
        return knapsackdpmpi(self.args)


class BenchmarkKnapsackDPGPU(BenchmarkTest):
    def onExecute(self) -> KnapsackSolution:
        return knapsackdpgpu(self.args)


class TestRegister:
    def __init__(self, save_file: Optional[str] = None, capacity: int = 0,
                 min_weight: int = 0, max_weight: int = 0, seed: int = 0) -> None:
        self._tests: Dict[str, BenchmarkTest] = {}
        self._save_file = save_file
        self._capacity = capacity
        self._min_weight = min_weight
        self._max_weight = max_weight
        self._seed = seed

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

        frontier_min = frontier_median = frontier_mean = frontier_max = 0
        if test.is_dag_test:
            stats = get_dag_stats()
            if stats.levels > 0:
                frontier_min = stats.frontierMin
                frontier_median = stats.frontierMedian
                frontier_mean = stats.frontierMean
                frontier_max = stats.frontierMax

        with open(self._save_file, mode='a', newline='') as f:
            writer = csv.writer(f)
            hostname = MPI.Get_processor_name() if test.is_mpi_test else os.uname().nodename
            if not file_exists:
                writer.writerow(['hostname','testname', 'testtype', 'time', 'processors', 'solution_weight', 'solution_profit', 'capacity', 'num_items', 'min_weight', 'max_weight', 'seed', 'frontier_min', 'frontier_median', 'frontier_mean', 'frontier_max'])
            writer.writerow([hostname, test_name, test_type, f"{duration:.4f}", processors, result.totalWeight, result.totalValue, self._capacity, test.numItems, self._min_weight, self._max_weight, self._seed, frontier_min, frontier_median, frontier_mean, frontier_max])

    def run(self, name: str = "all") -> None:
        if name == "all":
            tests_to_run = self._tests
        else:
            if name not in self._tests:
                raise ValueError(f"Test '{name}' not found")
            tests_to_run = {name: self._tests[name]}
        
        for test_name, test in tests_to_run.items():
            duration, result = test.execute()

            if test.is_mpi_test and MPI.COMM_WORLD.rank != 0:
                continue  # Only rank 0 prints/saves results for MPI tests

            print(f"{test_name}: {duration:.4f}s | Items: {test.numItems} | "
                  f"Profit: {result.totalValue} | Weight: {result.totalWeight}")

            sections = get_all_sections()
            if sections:
                print("  Time sections:")
                for name, sec in sorted(sections.items()):
                    print(f"    {name}: count={sec.count} mean={sec.mean:.4f}ms "
                          f"min={sec.min:.4f}ms max={sec.max:.4f}ms")

            stats = get_dag_stats()
            if stats:
                print(f"    DAG stats: tiles={stats.tiles} edges={stats.edges} "
                      f"levels={stats.levels}")
                if stats.levels > 0:
                    print(f"    wavefront widths: min={stats.frontierMin:.0f} "
                          f"median={stats.frontierMedian:.1f} mean={stats.frontierMean:.2f} "
                          f"max={stats.frontierMax:.0f}")

            if self._save_file:
                self._append_result(test_name, test, duration, result)



