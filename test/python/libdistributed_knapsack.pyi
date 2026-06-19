def hello() -> None: ...

class KnapsackResult:
    def __init__(self, totalValue: int, totalWeight: int):
        self.totalValue = totalValue
        self.totalWeight = totalWeight

class KnapsackArguments:
    def __init__(self, weights: list[int], values: list[int], capacity: int):
        self.weights = weights
        self.values = values
        self.capacity = capacity

def knapsackdp(weights: list[int], values: list[int], capacity: int, numThreads: int = 1) -> KnapsackResult: ...


