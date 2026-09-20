#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

#show: style-algorithm
= Knapsack DP Classic frontier

La risoluzione classica dell'algoritmo dello zaino in versione dynamic programming è sostanzialmente l'applicazione del seguente algoritmo

#KpDp

Questo algoritmo ha una complessità di $O(N * C)$ dove C è la capacità dello zaino, la frontiera di esecuzione lungo cui è possibile parallelizzare è sostanzialmente la linea corrente di calcolo. A prima vista si potrebbe pensare che l'algoritmo non stia garantendo correttamente l'ordine di esecuzione, ad un ispezione più accurata invece si può notare che il calcolo di ogni linea $i$ ha come unica dipendenza che il calcolo sulla linea $i-1$ sia stato eseguito e che tutti gli esecutori abbiano terminato ogni operazione sulla linea $i-1$ rendendo di fatto possibile la sua esecuzione su EREW PRAM (Exclusive Read, Exclusive Write) visto che tutti gli accessi in lettura avvengono solo sulla linea precedente ad opera di un singolo esecutore e in scrittura solo sulla linea corrente, anche qui ad opera di un singolo esecutore.


== Knapsack DP Classic frontier: implementazione

L'implementazione più semplice di questo algoritmo è la seguente.

#figure(
  caption: "Knapsack DP Classic frontier: implementazione semplice",
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

In questa implementazione ci sono due problemi, il primo è che l'allocazione della DP avviene come vettore contenente vettori, ogni accesso alla DP comporta l'accesso a un puntatore e questo pregiudica la località dei dati durante l'esecuzione; il secondo è che OpenMP a ogni iterazione aprirà e chiuderà i thread, con conseguente overhead. Per ovviare al problema si usa la seguente implementazione

#figure(
  caption: "Knapsack DP Classic frontier: Implementazione migliorata",
  kind: "listing",
  supplement: none,
  sourcecode[
    ```cpp
    int n = weights.size();
    	Eigen::MatrixX<int> dp{};
    	dp.resize(n + 1, capacity + 1);
    	dp.setZero();
    #pragma omp parallel
    	for (int i = 1; i <= n; ++i)
    	{
    		int tid = omp_get_thread_num();
    		int nt = omp_get_num_threads();
    		int begin = std::ceil(tid * capacity / nt);
    		int end = std::min<int>(capacity, std::ceil((tid + 1) * capacity / nt));
    		for (int w = begin; w <= end; ++w)
    		{
    			if (weights[i - 1] <= w)
    			{
    				dp(i, w) = std::max(dp(i - 1, w), dp(i - 1, w - weights[i - 1]) + values[i - 1]);
    			}
    			else
    			{
    				dp(i, w) = dp(i - 1, w);
    			}
    		}
    #pragma omp barrier
    	}
    ```
  ],
)

In questa implementazione si usa una Eigen matrix al posto di un vettore di vettori per rappresentare la DP, che viene allocata in row major order, il vantaggio è che questo oggetto garantisce accessi facili da leggere e un allocazione contigua in memoria della tabella, massimizzando in questo modo la località dei dati. La threadpool di OpenMP viene aperta nel ciclo for più esterno, in questo modo quando si procede a calcolare le righe della DP non avviene l'apertura e chiusura dei threads, invece si indica loro solo di attendere alla fine di ogni riga mediante una barriera.

== Knapsack DP Classic Frontier: Risultati

Per confrontare i risultati bisogna tenere conto di 3 parametri del problema che ne modificano

