#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

#show: style-algorithm
= DP basata su DAG

Un modello alternativo rappresenta la DP con un DAG. Dalla ricorrenza $"Dp"_i(w)=max("Dp"_(i-1)(w), "Dp"_(i-1)(w-w_i)+v_i)$, lo stato $(i,w)$ dipende da $(i-1,w)$ e, quando ammissibile, da $(i-1,w-w_i)$. A granularità di cella, dato l'insieme $C$ dei nodi completati, $"Ready"(C)={v in.not C : "Pred"(v) subset.eq C}$ contiene i nodi i cui predecessori sono già stati calcolati; questa frontiera dinamica riproduce le righe della soluzione classica.

Per aumentare la granularità si raggruppano righe e capacità in tile. Dati due tile distinti $A$ e $B$, si aggiunge $A arrow B$ se $"Pred"(B) inter A eq.not emptyset$, con $"Pred"(B)=union_(x in B) "Pred"(x)$. Il livello è una proprietà statica del vertice: $L(v)=0$ se non ha predecessori e $L(v)=1+max_(p in "Pred"(v))L(p)$ altrimenti. Nell'implementazione, i tile dello stesso livello formano una frontiera di esecuzione e possono essere calcolati in parallelo.

La rappresentazione delle dipendenze dello 0/1 Knapsack mediante DAG è discussa anche nel framework DPX10, che opera a granularità di cella schedulando dinamicamente le celle durante l'esecuzione; l'implementazione qui presentata adotta invece tile bidimensionali e una schedulazione OpenMP basata su livelli statici @dpx10.


#KpDpDag

L'algoritmo costruisce il grafo, assegna i livelli ed esegue in sequenza le frontiere corrispondenti. I vertici si suppongono in ordine topologico; nella costruzione seguente questa proprietà discende dall'inserimento row-major.


#GenerateDAG

Per il caso skip si legge $"DP"[i-1][w]$; il caso take legge $"DP"[i-1][w-w_i]$ quando l'indice è valido. Per un tile sull'intervallo $[c,c+g)$, le capacità lette dal caso take sono quindi $[c-w_i,c+g-w_i)$, troncate a $[0,C]$; gli intervalli vuoti non generano archi. `AddEdgesFromDependencyRanges` collega ogni tile predecessore che interseca questi intervalli, oltre al tile superiore del caso skip. Poiché i predecessori si trovano sopra oppure a sinistra, hanno descrittore row-major inferiore: non serve un ordinamento topologico esplicito.

#AssignLevels

L'assegnamento dei livelli applica la ricorrenza definita sopra, sfruttando il fatto che ogni predecessore è già stato visitato.

#ComputeTile

#ComputeTileSubroutines

I tile $(b,q)$ e le capacità $w=0 dots C$ usano indici da zero, gli oggetti $i=1 dots n$ da uno. La riga 0 di ciascun tile contiene il confine ereditato dal tile superiore; le righe successive applicano la DP agli oggetti locali. La routine principale coordina quattro procedure ausiliarie: `CopyBoundaryRow` inizializza la riga di confine, `CopySkipPrefix` copia le colonne in cui l'oggetto non entra, mentre `ComputeTakeFromLeft` e `ComputeTakeInside` risolvono il caso take con sorgente rispettivamente in un tile a sinistra o interna al tile.

== Implementazione

I tile sono vertici di un `boost::adjacency_list` bidirezionale @boostGraphAdjacencyList. `NodeData` descrive il sottoblocco associato. I parametri `item_block` e `cap_block` definiscono una griglia di $n_b = ceil(n\/"item"_b)$ per $n_q = ceil((C+1)\/"cap"_b)$ tile, con descrittore row-major $b dot n_q + q$. Ogni blocco è una matrice Eigen row-major con una riga di confine aggiuntiva; `dep_ranges` memorizza gli intervalli usati per costruire gli archi.

#figure(
  caption: "Implementazione DAG: strutture dati del grafo",
  kind: "listing",
  sourcecode[
    ```cpp
    using BlockMatrix = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    struct NodeData
    {
        int b, q;          // coordinate del tile nella griglia
        int rows;          // item locali + 1 (riga 0 = riga di confine)
        int width;         // colonne di capacità del tile
        int level;         // livello usato per formare la frontiera di esecuzione
        int lo, hi;        // intervallo di capacità posseduto
        std::vector<std::pair<int, int>> dep_ranges; // dipendenza per item locale
        BlockMatrix block; // blocco DP, dimensioni rows x width
    };

    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
        boost::property<boost::vertex_index_t, int, NodeData>>;
    ```
  ],
)



Nell'implementazione, `compute_levels` esegue un solo passaggio sui vertici e `solve_dag` raggruppa i tile per livello, eseguendo i livelli in sequenza e parallelizzando ciascuna frontiera.

#figure(
  caption: "Implementazione DAG: esecuzione per livelli",
  kind: "listing",
  sourcecode[
    ```cpp
    compute_levels(g); 
    int max_level = 0;
    for (int b = 0; b < nb; ++b)
        for (int q = 0; q < nq; ++q)
            max_level = std::max(max_level, g[b * nq + q].level);

    std::vector<std::vector<int>> by_level(max_level + 1);
    for (int b = 0; b < nb; ++b)
        for (int q = 0; q < nq; ++q)
            by_level[g[b * nq + q].level].push_back(b * nq + q);

    for (int L = 0; L <= max_level; ++L)
    {
        const std::vector<int> &tiles = by_level[L];
        // il calcolo sul livello successivo deve avvenire solo quando concluso quello corrente
        #pragma omp parallel for
        for (std::size_t t = 0; t < tiles.size(); ++t)
            compute_tile(g, tiles[t], weights, values, item_block, cap_block);
    }
    ```
  ],
)

La barriera implicita a fine `parallel for` completa il livello $L$ prima dell'avvio del livello $L+1$: ogni tile trova quindi completati tutti i predecessori appartenenti ai livelli precedenti. Il valore ottimo si legge infine nell'angolo in basso a destra dell'ultimo tile, `block(rows - 1, capacity - lo)`.

Il calcolo di un singolo tile (`compute_tile`) è la ricorrenza classica confinata al sottoblocco. La riga 0 viene copiata dal tile sopra: per ogni capacità $w$ dell'intervallo si risale al tile proprietario ($q' = w\/"cap"_b$, colonna $w - q' dot "cap"_b$) e se ne legge l'ultima riga, oppure si scrivono zeri se $b = 0$. Le righe interne dividono la riga in tre intervalli contigui: prima che l'item entri ($"lo" + c < w_i$, copia della riga sopra), poi il caso "take" con sorgente $w - w_i$ in un tile a sinistra della stessa riga, infine il caso "take" con sorgente interna al tile, che usa lo stesso ciclo di calcolo a puntatori delle altre varianti: i due estremi degli intervalli ($"lo" + c = w_i$ e $w - w_i = "lo"$) si calcolano una volta per riga.

La funzione di alto livello `knapsackdpdag` incolla le fasi: costruzione del grafo, risoluzione, ricostruzione della soluzione e al termine registra in `DAGStats` il numero di tile ($n_b dot n_q$), di archi e di livelli, insieme alla larghezza dei livelli (tile per livello, riassunta con media, mediana, minimo e massimo): sono le statistiche riportate in @tab:dag-frontiers nella sezione dei risultati.

== Risultati

=== Esecuzione dell'esperimento

Le misure condividono l'ambiente del capitolo precedente: nodo ARM Neoverse-N1 del cluster mercurio, `kpbench` compilato con GCC 14.2.0 in preset Release e misure `perf stat`. Tempi e contatori delle due implementazioni provengono dalla stessa campagna. Il confronto del solo calcolo usa `compute`, mentre il wall time include anche la costruzione e la deallocazione del grafo. Poiché le dipendenze derivano dai pesi, un'estensione potrebbe riutilizzare il grafo al variare dei soli profitti; questa possibilità non è stata misurata.

=== Scalabilità e crossover



#figure(
  image("../images/perf_kpdpdag_speedup_vs_n.png", width: 100%),
  caption: [Speedup di `compute` del solver DAG al variare di thread e $n$ (capacità $10^6$, geometria migliore per punto, mediana di 3 ripetizioni).],
) <fig:dag-speedup>

La @tab:dag-campaigns riassume la configurazione della campagna di @fig:dag-speedup.

#figure(
  table(
    columns: 5,
    [$n$], [$C$], [$W_"max"$], [$"item"_b$ × $"cap"_b$], [thread (ripetizioni)],
    [50], [$10^6$], [10], [(25, 10³) a 2-4-32 t, (10, 10³) a 8 t, (25, 10⁴) a 1 t], [1, 2, 4, 8, 16, 32 (3)],
    [1000], [$10^6$], [10], [(25, 10³) a 1-8 t, (10, 10⁴) a 16-32 t], [1, 4, 8, 16, 32 (3)],
    [5000], [$10^6$], [10], [(25, 10³) a 1 t, (25, 10⁴) a 4-32 t], [1, 4, 8, 16, 32 (3)],
  ),
  caption: [Configurazioni della campagna di @fig:dag-speedup; geometria migliore per punto sul wall time, mediana di 3 ripetizioni.],
) <tab:dag-campaigns>

La campagna riassunta in @tab:dag-campaigns fissa la capacità e varia il numero di oggetti. A $n=50$, `compute` non trae vantaggio da 32 esecutori perché i livelli delle geometrie selezionate contengono pochi tile. Per le taglie maggiori lo speedup di `compute` a 32 thread è circa $10.0$ a $n=1000$ e $11.3$ a $n=5000$, contro uno speedup ideale di $32$. Lo speedup del wall time è più limitato perché `first_touch` e `teardown` (@tab:dag-phases) coprono la maggior parte del tempo alle taglie grandi.

#figure(
  table(
    columns: 8,
    [$n$], [$"item"_b$], [$"cap"_b$], [tile], [livelli], [$"min"$], [$"mediana"$], [$"media"\/"max"$],
    [50], [5], [$10^2$], [100010], [10010], [1], [10.0], [9.99 \/ 10],
    [50], [5], [$10^4$], [1010], [110], [1], [9.9], [9.18 \/ 10],
    [50], [25], [$10^2$], [20002], [10002], [1], [2.0], [2.00 \/ 2],
    [50], [25], [$10^4$], [202], [102], [1], [2.0], [1.98 \/ 2],
    [5000], [5], [$10^2$], [10001000], [11000], [1], [993.2], [909.2 \/ 1000],
    [5000], [5], [$10^4$], [101000], [1100], [1], [100.3], [91.8 \/ 101],
    [5000], [25], [$10^2$], [2000200], [10200], [1], [199.9], [196.1 \/ 200],
    [5000], [25], [$10^4$], [20200], [300], [1], [74.6], [67.3 \/ 101],
  ),
  caption: [Statistiche dei livelli per geometrie rappresentative (larghezza = tile per livello).],
) <tab:dag-frontiers>

Le statistiche di @tab:dag-frontiers mostrano che la geometria dei tile modifica la larghezza dei livelli e quindi il parallelismo disponibile. L'inserimento row-major garantisce l'ordine topologico; nelle configurazioni analizzate, le frontiere sono disposte lungo anti-diagonali. I valori riportati sono quelli misurati per ciascuna configurazione.

Per interpretare il parallelismo disponibile si possono definire work e span. Sia $G=(V,E)$ il DAG e $c(v)$ il costo del tile $v$: il work è il costo complessivo, mentre lo span è il costo del cammino critico:

$ W = sum_(v in V) c(v), quad S = max_(P "cammino in" G) sum_(v in P) c(v). $

Ipotizzando che tutti i tile abbiano lo stesso costo, allora $W/S_"levels" = abs(V)/L$, dove $L$ è il numero di livelli: il parallelismo medio coinciderebbe quindi con la larghezza media riportata in tabella. Per esempio, a $n=5000$, con `item_block=25` e `cap_block=10^4`, si ottiene $20200/300 approx 67.3$. Tuttavia questa è un approssimazione, un modello più preciso richiederebbe una stima più accurata del costo di ogni span.



#figure(
  table(
    columns: 10,
    [$n$],
    [$W_"max"$],
    [$"item"_b$],
    [$"cap"_b$],
    [thread],
    [wall time (ms)],
    [`graph_` #linebreak() `build` (ms)],
    [`first_` #linebreak() `touch` (ms)],
    [`compute` (ms)],
    [`teardown` (ms)],

    [50], [10], [25], [$10^4$], [1], [98.1], [0.3], [65.0], [12.7], [20.2],
    [50], [10], [25], [$10^3$], [32], [44.2], [3.1], [8.2], [13.3], [19.5],
    [1000], [10], [10], [$10^4$], [32], [464.8], [14.1], [134.4], [42.6], [273.0],
    [5000], [10], [25], [$10^4$], [32], [2050.1], [38.5], [621.9], [193.2], [1194.6],
  ),
  caption: [Fasi del solver DAG su 4 configurazioni (mediana di 3 ripetizioni, geometria migliore per punto).],
) <tab:dag-phases>


Il solver registra separatamente la costruzione del DAG nella fase `graph_build`, i cui tempi nelle configurazioni analizzate sono riportati in @tab:dag-phases. Tali misure non sono sufficienti a ricavare una legge di crescita del costo rispetto al solo numero di tile. Poiché grafo e dipendenze sono costruiti a partire dai pesi degli oggetti, una possibile estensione futura è il riutilizzo del grafo quando vengono modificati soltanto i profitti, lasciando invariati i pesi: questa eventualità riguarda una diversa gestione del solver e non è stata misurata nella campagna presentata.

@tab:dag-phases mostra inoltre che, a 32 thread, `teardown` supera metà del wall time nelle configurazioni a taglia grande riportate. Nella griglia di @fig:dag-block-grid, relativa a $n=5000$, la stessa fase resta compresa fra circa $1.2$ e $1.4$ s al variare delle quattro geometrie. Questi dati ne documentano peso e stabilità, senza identificare il meccanismo che ne determina il costo né gli effetti di modifiche alla gestione della memoria.






=== L'effetto del tiling sulla località

#figure(
  image("../images/perf_vs_algorithms.png", width: 100%),
  caption: [Confronto fra le implementazioni classica e DAG a capacità $10^6$ e $n=50$],
) <fig:dag-vs-classic>

La @fig:dag-vs-classic mostra che in seriale il DAG presenta un IPC leggermente inferiore e circa $3.2$–$3.6$ volte più cicli/cella del classico. A 32 thread il rapporto si ribalta: l'IPC scende a circa $0.25$–$0.27$ nel classico, mentre resta compreso fra circa $1.8$ e $2.3$ nel DAG; i cicli/cella salgono a circa $58$ nel classico, contro valori di circa $36$–$44$ nel DAG. Anche le LLC MPKI mostrano una separazione marcata a 32 thread, con valori di $8.6$–$9.6$ per il classico contro $1.7$–$1.9$ per il DAG. Le due famiglie di pesi mostrano andamenti qualitativi coerenti. Tuttavia, poiché i contatori sono aggregati sull'intero processo, queste differenze non vanno interpretate come misure del solo `compute` né come prova di uno specifico meccanismo di località o sincronizzazione.


=== La granularità dei tile

#figure(
  image("../images/perf_kpdpdag_block_grid.png", width: 100%),
  caption: [Effetto di `item_block` e `cap_block` a $n=5000$, capacità $10^6$, 32 thread. Sopra: wall time, IPC e LLC MPKI; sotto: tempi di `first_touch`, `compute` e `teardown` (mediana di 3 ripetizioni, scala logaritmica).],
) <fig:dag-block-grid>

La @fig:dag-block-grid mostra l'effetto della geometria dei tile sul solver a $n=5000$ e 32 thread. `compute` occupa $170$-$210$ ms, meno di un decimo del wall time, mentre `first_touch` e `teardown` assorbono la maggior parte del tempo. `teardown` resta quasi costante, fra $1.2$ e $1.4$ s. `compute` varia poco e non in modo monotono: il suo minimo si ottiene a $(25,10^3)$, mentre il wall time minimo si ottiene a $(25,10^4)$. Anche gli indicatori hardware variano poco: l'IPC resta fra $1.32$ e $1.42$ e le miss di ultimo livello fra $10.4$ e $11.9$ MPKI. Questi contatori riguardano l'intero processo e non identificano un meccanismo specifico del calcolo o della gestione della memoria.

== Confronto fra le implementazioni classica e DAG

#figure(
  image("../images/perf_big_crossover.png", width: 100%),
  caption: [Tempi di `compute` $T_"plain"\/T_"dag"$ a capacità $10^6$ ($>1$: DAG più veloce).],
) <fig:kpdp-perf-crossover>

#figure(
  image("../images/perf_big_crossover_wall.png", width: 100%),
  caption: [Wall time $T_"plain"\/T_"dag"$ a capacità $10^6$ ($>1$: DAG più veloce).],
) <fig:dag-crossover-wall>

In @fig:kpdp-perf-crossover, a $n=50$ il DAG è più rapido in `compute` solo in seriale ($1.18 times$), scendendo a $0.18$ a 32 thread, dove la geometria scelta offre al massimo due tile per livello. A $n=1000$ e $n=5000$ il classico resta più veloce, con rapporti di $0.78$ e $0.84$ a 32 thread. Sul wall time, @fig:dag-crossover-wall mostra invece rapporti di $1.20$ e $1.40$ alle due taglie maggiori: le fasi esterne a `compute` compensano la fase di calcolo più lenta del DAG. A $n=50$ il classico resta più veloce anche sul wall time ($0.68$ a 32 thread).

#figure(
  image("../images/perf_big_comparison.png", width: 100%),
  caption: [Contatori hardware delle due implementazioni a $n=5000$, capacità $10^6$, `maxWeight=10`; geometria migliore per punto.],
) <fig:dag-big-comparison>

A $n=5000$, @fig:dag-big-comparison conferma la divergenza dei contatori: a 32 thread il classico scende da circa $1.8$ a $0.26$ IPC e supera $50$ cicli/cella, mentre il DAG resta a circa $1.36$ IPC e $9$ cicli/cella. A 16 e 32 thread il classico registra però meno LLC MPKI. Questi contatori aggregati non contraddicono il risultato temporale: il classico resta più rapido in `compute`, ma è più lento sul wall time.

=== Robustezza del wall time del DAG rispetto al seed

#figure(
  image("../images/perf_kpdpdag_seed9_wall.png", width: 85%),
  caption: [Wall time del DAG a $n=5000$, capacità $10^6$, $W_"max"=10$: mediana delle mediane per seed e banda minimo–massimo fra nove seed.],
) <fig:dag-seed-wall>

La @fig:dag-seed-wall estende a nove seed il confronto sul wall time. Per ogni geometria e seed sono state effettuate cinque ripetizioni. Il wall time diminuisce sensibilmente fino a 8 thread in tutte le geometrie, mentre il guadagno aggiuntivo a 32 thread è più limitato e dipende dalla geometria. Il grafico riguarda soltanto il wall time del solver e non permette di separare `compute`, `first_touch` e `teardown`.
