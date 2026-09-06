#import "../functions/preamble.typ": *

= Knapsack DP Naive frontier

La risoluzione classica dell'algoritmo dello zaino in versione dynamic programming è sostanzialmente l'applicazione del seguente algoritmo

#show: style-algorithm
#algorithm-figure("Knapsack DP: Naive frontier parallel", {
  import algorithmic: *
  Procedure("Knapsack DP", ("w:[]", "v:[]", "C: int"), {
    Assign[$n$][$"len(w)"$]
    Assign[$"dp"$][$"[]"$]
    For("i to n in parallel", {
      IfElseChain(
        $"weights"[i-1] <= w$,
        {
          Assign[$"dp[i][w]"$][$"max(dp[i-1][w],dp[i-1][w-weights[i-1]] + values[i-1])"$]
        },
        {
          Assign[$"dp[i][w]"$][$"dp[i-1][w]"$]
        },
      )
    })
  })
})

Questo algoritmo ha una complessità di $O(N * C)$ dove C è la capacità dello zaino, la parallelizzazione avviene lungo la linea corrente che si sta valutando, un approccio naive al problema che si può risolvere mediante l'aggiunta di un semplice pragma omp al ciclo for interno che esegue il calcolo sulla linea.



== Knapsack DP Naive frontier: implementazione


#figure(
  caption: "Knapsack dynamic programming Naive frontier",
  kind: "listing",
  supplement: none,
  sourcecode[
    ```cpp
    int n = weights.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(capacity + 1, 0));
    // Build the dp table
    for (int i = 1; i <= n; ++i) {
        #pragma omp parallel for
        for (int w = 0; w <= capacity; ++w) {
            if (weights[i - 1] <= w) {
                dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
            } else {
                dp[i][w] = dp[i - 1][w];
            }
        }
    }
    ```
  ],
)

== Knapsack DP Naive Frontier: Risultati




