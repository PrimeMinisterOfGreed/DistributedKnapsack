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

Questo algoritmo ha una complessità di $O(N * C)$ dove C è la capacità dello zaino, la parallelizzazione avviene lungo la linea corrente che si sta valutando, un approccio naive al problema ,che utilizzando un modello PRAM,si può risolvere mediante l'aggiunta di un semplice pragma omp al ciclo for interno che esegue il calcolo sulla linea. In questo caso il modello PRAM adottato è EREW (Exclusive Read , Exclusive write), gli accessi in scrittura avvengono sulla frontiera (linea) corrente mentre le letture esclusivamente da quella precedente, ogni thread accede ad uno spazio diverso e non c'é nessuna concorrenza.



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

Come si può vedere da questo listing, il risultato è piuttosto lineare anche nel codice, la dp è condivisa come vettori di vettori, il vettore ha come proprietà quella di garantire la contiguità dei dati.

== Knapsack DP Naive Frontier: Risultati


= Knapsack DP DAG Model

Un modello alternativo di esecuzione per questo algoritmo è pensare di creare un Directed Acyclic Graph che rappresenti la DP invece di usare una matrice. Per farlo si consideri la ricorrenza $"Dp"_i(w)=max("Dp"_(i-1)(w), "Dp"_(i-1)(w-w_i)+v_i)$, allora si può raggiungere uno stato $(i,w)$ da $(i-1,w)$ e da $(i-1,w-w_i)$, questo insieme di archi impone sostanzialmente l'ordine di esecuzione dell'algoritmo sulle diverse celle della DP e dipende solo da $w_i$. A questo punto si possono definire le frontiere di questo DAG, in modo semplice, come l'insieme degli stati che possono essere calcolati in parallelo, dato che non dipendono da altri stati, formalmente dati due stati $u,v in F$ allora $u arrow.not v and v arrow.not u$ che formano quindi una anti chain.

Costruendo una frontiera molto semplice inizialmente, ad esempio $F_0={(0,0),(0,1),...,(0,W)}$ si possono costruire le frontiere successive utilizzando i successori dei nodi calcolati, dato un insieme di nodi completati $C$ questo insieme $"Ready"(C)={v in.not C : "Pred"(v) subset.eq C}$ forma la frontiera di nodi successiva. Se si suppone un parallelismo a livello di cella si scopre in fretta che si riproduce praticamente l'esecuzione dell'algoritmo "naive", infatti $F_1$ sarà $F_1={(1,0),(1,1),...,(1,W)}$.

Per ottenere una soluzione diversa, computazionalmente parlando, è necessario considerare un parallelismo con granularità più grossolana della cella, per farlo si può dividere la DP a blocchi (o tile). Partendo dal caso più semplice si può pensare di raggruppare porzioni della prima riga, che ospita sostanzialmente tutti i nodi della frontiera iniziale in blocchi, ora ripetendo il ragionamento precedente si può costruire nuovamente la frontiera iniziale cambiando però la condizione, per ogni blocco $B_(i,j)$ se esiste una cella $w in B_(i,j)$ che ha come dipendenza una cella di un altro blocco allora il blocco è dipendente da quel blocco.
