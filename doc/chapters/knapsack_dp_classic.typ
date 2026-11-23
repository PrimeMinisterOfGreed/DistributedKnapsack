#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

#show: style-algorithm
= Knapsack DP Classic frontier

La risoluzione classica dell'algoritmo dello zaino in versione dynamic programming è sostanzialmente l'applicazione del seguente algoritmo

#KpDp <algo-kpdp>

Questo algoritmo ha una complessità di $O(N * C)$ dove C è la capacità dello zaino, la frontiera di esecuzione lungo cui è possibile parallelizzare è sostanzialmente la linea corrente di calcolo. Le celle della riga $i$ dipendono solo dalla riga $i - 1$ e ogni esecutore *scrive* celle diverse; le letture invece non sono esclusive: la cella $"DP"[i-1][x]$ è letta sia dalla cella $w = x$ (caso "non prendo") sia dalla cella $w = x + w_i$ (caso "prendo"), in generale assegnate a esecutori diversi. Il modello di riferimento è quindi la CREW PRAM (Concurrent Read, Exclusive Write)


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

In questa implementazione il problema è che OpenMP apre e chiude il team di thread a ogni riga, con un overhead che si paga $n$ volte. Per mitigare l'overhead si mantiene il team aperto per tutto il ciclo sulle righe, con una barriera a fine riga. Per rendere esplicito come la matrice viene allocata, si usa la libreria Eigen3, che permette di allocare una matrice come array contiguo e di indicare l'ordine di allocazione (row-major o column-major). Nell'esempio precedente con `std::vector<std::vector<int>>`, invece, la contiguità è garantita soltanto all'interno dei singoli vettori di riga, non fra righe diverse.

#figure(
  caption: "Knapsack DP Classic frontier",
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

La threadpool di OpenMP viene aperta nel ciclo for più esterno, in questo modo quando si procede a calcolare le righe della DP non avviene l'apertura e chiusura dei threads, invece si indica loro solo di attendere alla fine di ogni riga mediante una barriera.

== Knapsack DP Classic Frontier: Risultati

Per confrontare i risultati bisogna tenere conto di 3 parametri del problema che ne modificano profondamente il calcolo. Il primo è la capacità massima dello zaino, che cambia la dimensione della riga; il secondo è il range (e il seed di generazione) dei pesi, un peso grande richiede al processore di eseguire un salto più o meno grande nella riga precedente; l'ultimo è il numero di oggetti, che rende sostanzialmente la DP più o meno lunga, in questo caso comporta essenzialmente un incremento lineare del tempo, ma se si confrontano i tempi con la versione DAG che invece accorpa anche le linee è un parametro che è utile considerare. La granularità del problema in questo caso è dipendente dalla capacità totale, ad ogni thread viene assegnata una porzione della riga divisa in parti uguali tra threads.



=== Esecuzione dell'esperimento
L'algoritmo è stato fatto girare su una CPU Neoverse-N1, basata su architettura ARM, con 80 core in un unico socket: ciascun core ha 64 KB di cache di primo livello dati ($approx 5$ MB aggregati) e 1 MB di cache di secondo livello privata ($approx 80$ MB sommati fra 80 core). Il programma è stato compilato con GCC 14.2.0 e il preset Release di CMake, che usa i flag di ottimizzazione predefiniti (-O3).

#figure(
  image("../images/kpdp_speedup_vs_processors.png", width: 100%),
  caption: [Speedup della fase compute del KnapsackDP classico per numero di oggetti e capacity.],
) <fig:kpdp-speedup>

#figure(
  table(
    columns: 8,
    [$n$], [$C$], [thread], [wall (ms)], [first touch (ms)], [% wall], [compute (ms)], [% wall],
    [50], [$10^6$], [1], [29.0], [13.1], [45%], [15.0], [52%],
    [50], [$10^6$], [32], [32.7], [29.4], [90%], [2.4], [7%],
    [50], [$10^7$], [32], [82.1], [53.6], [65%], [22.2], [27%],
    [5000], [$10^6$], [32], [3003], [2785], [93%], [162], [5%],
  ),
  caption: [Fasi del solver classico su 4 configurazioni (mediana di 3, maxWeight 10): nei run paralleli il wall è dominato dal first touch, il compute resta minoritario.],
) <tab:kpdp-phases>

Lungo tutto la trattazione si divideranno i tempi in sostanzialmente 3 sezioni: fase di setup o first_touch dove sostanzialmente si costruisce e azzera la memoria; fase di compute dove si esegue il kernel e fase di teardown dove si dealloca la memoria. Questa suddivisione è necessaria per facilitare la comparazione dei tempi tra le 2 soluzioni, perchè pagano costi diversi nelle varie fasi e spesso si è interessati a comparare la sola fase di lavoro, lasciando perdere le altre due, alla fine seguirà comunque una comparazione che prenderà in esame tutte le fasi.

La figura mostra lo speedup della fase compute, mentre i tempi di campagna e i contatori della sezione seguente coprono l'intero solver, first touch della tabella incluso. Il kernel scala in modo regolare in tutte le configurazioni: la curva sale al crescere della taglia del problema, pur restando lontana dallo speedup ideale al crescere dei thread. La regressione che si osserva invece nei wall time non è dunque del calcolo: a dominare il wall in parallelo è la fase di first touch della tabella, come mostra la ripartizione di @tab:kpdp-phases. Nei run paralleli il first touch non solo smette di scalare ma arriva a costare più dell'intero run seriale: toccare la tabella con molti thread li fa collidere sul sistema di pagine, e il costo della collisione cresce con il numero di esecutori, fino ad assorbire quasi tutto il wall e lasciare il compute in una frazione marginale del tempo; alla taglia grande il first touch riesce a sfruttare una parte del parallelismo ma resta comunque la voce dominante del wall.

#figure(
  table(
    columns: 8,
    [$n$], [$C$], [thread], [cicli (G)], [istruzioni (G)], [IPC], [LLC MPKI], [cicli/cella],
    [50], [$10^6$], [1], [0.11], [0.18], [1.60], [16.9], [2.2],
    [50], [$10^6$], [32], [2.98], [0.75], [0.25], [9.6], [58.4],
    [50], [$10^7$], [32], [7.17], [2.49], [0.35], [13.6], [14.1],
    [5000], [$10^6$], [32], [282.0], [74.5], [0.26], [4.9], [56.4],
  ),
  caption: [Contatori `perf` delle run di @tab:kpdp-phases (mediana di 3, intero processo): nei run paralleli i cicli si moltiplicano con le istruzioni quasi ferme e l'IPC crolla, mentre le miss di ultimo livello restano vicine al seriale.],
) <tab:kpdp-perf-counters>

Nei run paralleli i contatori di @tab:kpdp-perf-counters mostrano un aumento dei cicli rispetto al run seriale e un crollo dell'IPC aggregato. Poiché queste misure riguardano l'intero processo, non consentono di attribuire il degrado a una singola fase né di distinguere i contributi di compute, first touch e sincronizzazione: per analizzare la ripartizione dei tempi si fa quindi riferimento a @tab:kpdp-phases.

=== Script di esecuzione

La campagna di misure è stata eseguita sul cluster mercurio sulla partizione epito mediante il seguente script Slurm: per ogni combinazione di parametri (famiglia di peso, numero di thread, capacity) lo script esegue 3 repliche del solver sotto `perf stat`, che raccoglie i contatori hardware; il tempo del solver viene letto dallo stdout di `kpbench` della stessa esecuzione, così che contatori e tempi appartengano sempre allo stesso run, e ogni configurazione produce una riga nel CSV di riepilogo con la mediana calcolata in fase di analisi. I thread OpenMP vengono fissati uno per core con pinning (`OMP_PROC_BIND=close`, `OMP_PLACES=cores`): senza binding il sistema operativo può migrare i thread fra core durante l'esecuzione, e ogni migrazione invalida le cache del core di destinazione e rende il tempo di un kernel memory-bound dipendente da dove lo scheduler colloca i processi; con il binding ogni thread resta sul suo core, i tempi sono ripetibili fra repliche e i contatori restano attribuiti allo stesso core per tutta la misura.

#figure(
  caption: "sbatch della campagna di misure sul DP classico",
  kind: "listing",
  sourcecode[
    ```bash
    #!/bin/bash
    #SBATCH --job-name=kpdp_perf
    #SBATCH --ntasks=1
    #SBATCH --cpus-per-task=32
    #SBATCH --time=00:30:00
    #SBATCH --mem=32G
    set -euo pipefail
    SEED=42
    NUM_ITEMS=50
    BIN="./build/kpbench"
    REPEATS=3
    PERF_EVENTS="cycles,instructions,cache-references,cache-misses,
    branch-misses,L1-dcache-loads,L1-dcache-load-misses,ll_cache_rd,
    ll_cache_miss_rd,dTLB-load-misses,iTLB-load-misses"

    export LC_ALL=C

    # Pin the OpenMP threads (one per core, compact): without it threads migrate and
    # the timings of a memory-bound kernel depend on where the scheduler puts them.
    export OMP_PROC_BIND="${OMP_PROC_BIND:-close}"
    export OMP_PLACES="${OMP_PLACES:-cores}"

    for max_weight in 10 1000; do
      for threads in 1 8 32; do
        for cap in 1000000 10000000; do
          export OMP_NUM_THREADS="$threads"

          for rep in $(seq 1 "$REPEATS"); do
            RUN_OUT=$(perf stat -x , -e "$PERF_EVENTS" \
              -o "$OUT_DIR/perf_${max_weight}_cap${cap}_t${threads}_r${rep}.csv" \
              "$BIN" \
                --test "knapsackdp" \
                --capacity "$cap" \
                --numItems "$NUM_ITEMS" \
                --seed "$SEED" \
                --numThreads "$threads" \
                --maxWeight "$max_weight" \
                --save "")

            # il tempo del solver viene letto dal stdout della stessa esecuzione
            time_s=$(printf '%s\n' "$RUN_OUT" | sed -n "s/^knapsackdp: \([0-9.]*\)s .*/\1/p")
          done
        done
      done
    done

    ```
  ],
) <lst:kpdp-perf-sbatch>



