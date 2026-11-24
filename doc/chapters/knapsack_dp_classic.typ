#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

#show: style-algorithm
= DP classico: parallelizzazione per riga

La soluzione classica dello zaino mediante programmazione dinamica applica il seguente algoritmo:

#KpDp <algo-kpdp>

L'algoritmo ha complessità $O(n C)$, dove $n$ è il numero di oggetti e $C$ la capacità dello zaino; la frontiera di esecuzione è la riga corrente della tabella. Le celle della riga $i$ dipendono solo dalla riga $i-1$: ogni esecutore scrive celle diverse, mentre $"DP"[i-1][x]$ può essere letta sia per $w=x$ sia per $w=x+w_i$, in genere da esecutori diversi. Il modello di riferimento è quindi Concurrent Read, Exclusive Write (CREW PRAM).


== Implementazione

L'implementazione naïve usa una matrice `std::vector<std::vector<int>>` e una direttiva `omp parallel for` per ogni riga. Aprire e chiudere la regione parallela a ogni oggetto introduce overhead di gestione e sincronizzazione, anche se il runtime OpenMP può riutilizzare i thread della pool.

La variante analizzata usa Eigen3, con una matrice row-major allocata come unico array contiguo, e mantiene la regione `parallel` aperta all'esterno del ciclo. Ogni esecutore elabora sempre la stessa porzione di riga; la barriera è necessaria perché la riga successiva possa leggere celle scritte da altri esecutori @openmp52.

#figure(
  caption: "DP classico: implementazione Eigen/OpenMP",
  kind: "listing",
  supplement: none,
  sourcecode[
    ```cpp
    // Tabella ROW-major: la riga e' uno stream contiguo
    using DpTable = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    DpTable dp{}; dp.resize(n + 1, cols);   // solo allocazione: niente setZero() seriale
    #pragma omp parallel
    {
        auto [begin, end] = capacity_slice(cols, omp_get_thread_num(), omp_get_num_threads());
        for (int i = 0; i <= n; ++i) 
            for (std::int64_t w = begin; w < end; ++w)
                dp.row(i).data()[w] = 0;
        for (int i = 1; i <= n; ++i)
        {
            const int wi = weights[i - 1], vi = values[i - 1];
            const int *prev = dp.row(i - 1).data();
            int *cur = dp.row(i).data();
            for (std::int64_t w = begin; w < end; ++w)
                cur[w] = (wi <= w) ? std::max(prev[w], prev[w - wi] + vi) : prev[w];
            #pragma omp barrier   // la riga i+1 legge celle arbitrarie della riga i
        }
    }
    ```
  ],
)



== Risultati

Il confronto fra le implementazioni dipende da tre parametri: la capacità $C$, che determina la lunghezza della riga; la famiglia e il seed di generazione dei pesi, che influenzano gli accessi alla riga precedente; il numero di oggetti $n$, che determina il numero di righe. Nel solver classico ogni thread elabora una porzione di riga; nel DAG è inoltre rilevante la geometria del tiling, che raggruppa più righe.



=== Esecuzione dell'esperimento
Il solver è stato eseguito su una CPU Neoverse-N1, basata su architettura ARM, con 80 core in un unico socket: ciascun core ha 64 KB di cache di primo livello dati ($approx 5$ MB aggregati) e 1 MB di cache di secondo livello privata ($approx 80$ MB sommati fra 80 core). Il programma è stato compilato con GCC 14.2.0 e il preset Release di CMake, che usa i flag di ottimizzazione predefiniti (-O3).

=== Campionamento e aggregazione dei dati

Le misure distinguono la variabilità fra ripetizioni della stessa configurazione dalla sensibilità all'istanza, variando il seed. Per la variabilità fra ripetizioni si riportano mediana e intervallo interquartile (IQR). Il wall time del solver (`time_s`) è il tempo complessivo dell'esecuzione. La @tab:misure-campionamento riassume i gruppi confrontati.

#figure(
  table(
    columns: 5,
    [Solver, campione], [Gruppi di analisi], [Ripetizioni per gruppo], [Esecuzioni totali], [Ruolo],
    [GPU, seed 42], [36 configurazioni], [21], [756], [Mediana e IQR per configurazione],
    [DP classico, seed 42], [12 configurazioni], [21], [252], [Mediana e IQR del wall time],
    [DAG, per ogni taglia], [12 geometrie], [21], [252], [Mediana e IQR per geometria],
    [GPU, 9 seed], [324 gruppi configurazione–seed], [5 per seed], [1620], [Mediana delle mediane per seed],
    [DAG, 9 seed], [108 gruppi geometria–thread–seed], [5 per seed], [540], [Mediana delle mediane per seed],
  ),
  caption: [Riepilogo del campionamento usato per valutare ripetibilità e sensibilità all'istanza],
) <tab:misure-campionamento>

Sullo stesso seed si valuta la variabilità temporale fra esecuzioni; fra i seed si considera invece prima la mediana delle ripetizioni di ogni istanza, poi la distribuzione fra istanze. Nei confronti fra geometrie o block size, le differenze sono accoppiate per configurazione e seed. Questa analisi descrive il campione misurato, non tutti i possibili input o ambienti di esecuzione.

#figure(
  image("../images/kpdp_speedup_vs_processors.png", width: 100%),
  caption: [Speedup della fase `compute` dell'implementazione classica per numero di oggetti e capacità.],
) <fig:kpdp-speedup>

#figure(
  table(
    columns: 8,
    [$n$], [$C$], [thread], [wall time (ms)], [`first_touch` (ms)], [% wall time], [`compute` (ms)], [% wall time],
    [50], [$10^6$], [1], [29.0], [13.1], [45%], [15.0], [52%],
    [50], [$10^6$], [32], [32.7], [29.4], [90%], [2.4], [7%],
    [50], [$10^7$], [32], [82.1], [53.6], [65%], [22.2], [27%],
    [5000], [$10^6$], [32], [3003], [2785], [93%], [162], [5%],
  ),
  caption: [Fasi del solver classico su 4 configurazioni (mediana di 3 ripetizioni, `maxWeight=10`): nei run paralleli il wall time è dominato da `first_touch`, mentre `compute` resta minoritario.],
) <tab:kpdp-phases>

Per confrontare le implementazioni si distinguono le seguenti fasi temporizzate dal solver:

- *allocazione*: acquisizione della memoria, quando registrata separatamente;
- *first touch* (`first_touch`): azzeramento e primo accesso alle strutture della tabella DP;
- *costruzione del grafo* (`graph_build`): creazione di vertici, archi e strutture dati del DAG;
- *calcolo DP* (`compute`): intervallo temporizzato della fase di calcolo;
- *ricostruzione della soluzione* (`backtrack`): individuazione degli oggetti selezionati;
- *deallocazione* (`teardown`): rilascio delle strutture gestite dal solver;
- *statistiche*: raccolta delle informazioni sul DAG, quando richiesta.

Nell'implementazione classica, `compute` comprende la regione parallela che calcola le righe DP e le barriere necessarie fra una riga e la successiva. Non tutte le fasi sono presenti in ogni implementazione: `graph_build` e la raccolta delle statistiche sono specifiche del solver DAG. Questa distinzione consente di confrontare separatamente la fase di calcolo e il wall time del solver.

La @fig:kpdp-speedup mostra lo speedup di `compute`, mentre i tempi di campagna e i contatori seguenti coprono l'intero solver, incluso `first_touch`. La fase di calcolo scala in modo regolare all'aumentare della taglia, pur restando lontana dallo speedup ideale al crescere dei thread. La regressione del wall time non riguarda quindi `compute`: @tab:kpdp-phases mostra che nei run paralleli `first_touch` non segue la riduzione del calcolo e a 32 thread diventa la componente dominante del wall time, arrivando a superare l'intero run seriale. I timer documentano questa ripartizione, ma non identificano il meccanismo responsabile della regressione di `first_touch`.

#figure(
  table(
    columns: 8,
    [$n$], [$C$], [thread], [cicli (G)], [istruzioni (G)], [IPC], [LLC MPKI], [cicli/cella],
    [50], [$10^6$], [1], [0.11], [0.18], [1.60], [16.9], [2.2],
    [50], [$10^6$], [32], [2.98], [0.75], [0.25], [9.6], [58.4],
    [50], [$10^7$], [32], [7.17], [2.49], [0.35], [13.6], [14.1],
    [5000], [$10^6$], [32], [282.0], [74.5], [0.26], [4.9], [56.4],
  ),
  caption: [Contatori `perf` delle run di @tab:kpdp-phases (mediana di 3 ripetizioni, intero processo): nei run paralleli i cicli si moltiplicano con le istruzioni quasi ferme e l'IPC crolla, mentre le miss di ultimo livello restano vicine al seriale.],
) <tab:kpdp-perf-counters>

Nei run paralleli i contatori di @tab:kpdp-perf-counters mostrano un aumento dei cicli rispetto al run seriale e un crollo dell'IPC aggregato. Poiché queste misure riguardano l'intero processo, non consentono di attribuire il degrado a una singola fase né di distinguere i contributi di `compute`, `first_touch` e sincronizzazione: per analizzare la ripartizione dei tempi si fa quindi riferimento a @tab:kpdp-phases.

=== Acquisizione dei tempi e dei contatori

Gli script `sbatch` del repository eseguono `kpbench` sotto `perf stat`, variando famiglia di pesi, numero di thread e capacità. Nella campagna con contatori hardware ogni configurazione ha tre ripetizioni; tempi del solver e contatori provengono dalla stessa esecuzione. Il pinning compatto (`OMP_PROC_BIND=close`, `OMP_PLACES=cores`) limita le migrazioni dei thread @openmp52. I contatori restano aggregati sull'intero processo e non rappresentano misure separate per core o per singola fase.



