#import "../functions/preamble.typ": *

#show: style-algorithm
= Knapsack DP GPU

La versione su GPU (NVIDIA, tramite CUDA) dell'algoritmo dello zaino in versione dynamic programming riprende la frontiera di esecuzione del capitolo classico: la pseudocodifica è infatti la stessa presentata in @algo-kpdp, cambia sostanzialmente la macchina su cui la frontiera viene eseguita. Invece di un insieme di thread della CPU che collaborano su una riga della DP, ogni cella della riga corrente viene assegnata a un thread del grid CUDA; l'accesso agli elementi $w$ della riga è contiguo, quindi i thread di uno stesso warp effettuano accessi alla memoria globale e la ricorrenza non richiede strutture di supporto aggiuntive.

== Knapsack DP GPU: implementazione

Il cuore dell'implementazione è il kernel che calcola una riga della DP, ogni thread calcola esattamente una cella.

#figure(
  caption: "Knapsack DP GPU: kernel del passo di DP",
  kind: "listing",
  sourcecode[
    ```cpp
    __global__ void knapsack_step_kernel(const int *__restrict__ prev, int *__restrict__ cur, int weight, int value,
                                         int cols)
    {
        const int w = blockIdx.x * blockDim.x + threadIdx.x;
        if (w >= cols)
        {
            return;
        }
        const int exclude = prev[w];
        const int include = (weight <= w) ? prev[w - weight] + value : exclude;
        cur[w] = include > exclude ? include : exclude;
    }
    ```
  ],
)



#figure(
  caption: "Knapsack DP GPU: loop host con un lancio kernel per riga",
  kind: "listing",
  sourcecode[
    ```cpp
    const int cols = capacity + 1;
    const std::size_t tableSize = static_cast<std::size_t>(n + 1) * cols;

    const int threads = resolve_threads();
    const int blocks = default_blocks(threads, cols);
    int *d_dp = nullptr;

    cuda_check(cudaMalloc(&d_dp, tableSize * sizeof(int)), "cudaMalloc");
    // Only row 0 must be zero (dp[0][w] = 0); rows 1..n are fully overwritten.
    cuda_check(cudaMemset(d_dp, 0, static_cast<std::size_t>(cols) * sizeof(int)), "cudaMemset");
    cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

    const auto dpStart = std::chrono::steady_clock::now();

    for (int i = 1; i <= n; ++i)
    {
        const int *prev = d_dp + static_cast<std::size_t>(i - 1) * cols;
        int *cur = d_dp + static_cast<std::size_t>(i) * cols;
        knapsack_step_kernel<<<blocks, threads>>>(prev, cur, weights[i - 1], values[i - 1], cols);
        cuda_check(cudaGetLastError(), "knapsack_step_kernel");
    }
    cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

    const auto dpEnd = std::chrono::steady_clock::now();
    const double dpMs = std::chrono::duration<double, std::milli>(dpEnd - dpStart).count();
    g_last_dp_time_ms = dpMs;
    std::printf("GPU DP execution: %.3f ms\n", dpMs);
    ```
  ],
)

Il kernel è la traduzione più diretta possibile della frontiera classica e la riga precedente viene letta direttamente dalla memoria globale. Per ogni cella il kernel legge `prev[w]` e, quando l'item è ammissibile, legge anche `prev[w-weight]` sommandovi il valore dell'item; infine scrive `cur[w]`. Il confronto fra i due valori e l'eventuale addizione comportano poche operazioni aritmetiche per accesso in memoria. Le misure della campagna non riportano però la banda effettiva o altri contatori specifici dei sottosistemi di memoria, e non consentono quindi di classificare con certezza il kernel come memory bound.

La sincronizzazione è garantita implicitamente dal modello di esecuzione degli stream: i kernel consecutivi lanciati sullo stesso stream vengono eseguiti in ordine, quindi il kernel della riga $i$ vede la riga $i-1$ completa prima di partire. La dimensione del blocco è configurable mediante la variabile d'ambiente `KP_GPU_THREADS` (default 256) e viene limitata al massimo supportato dal device (`cudaDevAttrMaxThreadsPerBlock`).


== Knapsack DP GPU: Risultati

=== Esecuzione dell'esperimento

La campagna di misure è stata eseguita sul cluster mercurio con lo script Slurm riportato di seguito: per ogni combinazione di famiglia di peso ($"maxWeight" in {10, 1000}$), thread per blocco ($"KP_GPU_THREADS" in {32, 64, 128, 256, 512, 1024}$) e capacity ($10^5$, $10^6$, $10^7$) vengono ripetute 3 misure; per ogni replica lo script raccoglie dallo stdout di `kpbench` sia il tempo di esecuzione (wall time) del solver sia l'intervallo `dp_ms` del ciclo DP, apponendoli a un CSV di riepilogo. La partizione espone una NVIDIA A100-PCIE-40GB (40 GB di memoria HBM2e, banda di picco dichiarata $approx 1.55" TB"\/"s"$). Si noti che la griglia contiene sempre $ceil((C+1)/t)$ blocchi, cioè un thread per cella: il parallelismo totale è fisso, il numero di thread per blocco cambia solo la geometria di lancio e l'occupazione delle SM. Le taglie in $"KP_GPU_THREADS"$ seguono proprio la struttura dell'hardware: i thread di un blocco vengono raggruppati ed emessi in warp di 32 lane, l'unità di schedulazione dell'SM, quindi si considerano solo multipli di 32; una taglia non allineata genererebbe un ultimo warp parzialmente vuoto che consuma comunque uno slot di schedulazione.

`time_s` è il wall time del solver rilevato dal chiamante. Il valore `dp_ms` è invece un intervallo misurato sull'host, tra l'inizio del ciclo dei lanci DP e la conclusione della sincronizzazione finale: comprende i lanci dei kernel, la loro esecuzione sul device e la sincronizzazione finale, e non rappresenta il tempo puro dei kernel CUDA. Sono esclusi da questo intervallo l'allocazione iniziale della tabella e la `cudaMemset`, che precedono il suo avvio, nonché allocazioni, trasferimenti, backtrack e deallocazioni successivi alla sua conclusione.

#figure(
  caption: "sbatch della campagna di misure sulla versione GPU",
  kind: "listing",
  sourcecode[
    ```bash
    #!/bin/bash
    #SBATCH --job-name=knapsack_gpu
    #SBATCH --ntasks=1
    #SBATCH --cpus-per-task=8
    #SBATCH --gres=gpu:1
    #SBATCH --time=02:00:00
    #SBATCH --output=gpu.out
    #SBATCH --error=gpu.err
    #SBATCH --mem=32G

    set -euo pipefail

    SEED=42
    NUM_ITEMS=50
    BIN="./build/kpbench"
    REPEATS=3
    TEST="knapsackdp_gpu"
    OUT_DIR="gpu_out"
    SUMMARY="$OUT_DIR/gpu_summary.csv"

    mkdir -p "$OUT_DIR"
    echo "max_weight,capacity,threads,repeat,time_s,dp_ms" > "$SUMMARY"

    for max_weight in 10 1000; do
      for threads in 32 64 128 256 512 1024; do
        for cap in 100000 1000000 10000000; do
          export KP_GPU_THREADS="$threads"
          for rep in $(seq 1 "$REPEATS"); do
            RUN_OUT=$("$BIN" --test "$TEST" --capacity "$cap" \
              --numItems "$NUM_ITEMS" --seed "$SEED" \
              --maxWeight "$max_weight" --save "")
            time_s=$(printf '%s\n' "$RUN_OUT" | \
              sed -n "s/^${TEST}: \([0-9.]*\)s .*/\1/p")
            dp_ms=$(printf '%s\n' "$RUN_OUT" | \
              sed -n "s/^GPU DP execution: \([0-9.]*\) ms/\1/p")
            echo "$max_weight,$cap,$threads,$rep,$time_s,$dp_ms" >> "$SUMMARY"
          done
        done
      done
    done
    ```
  ],
) <lst:kpdpgpu-sbatch>

=== Effetto dei thread per blocco

#figure(
  image("../images/perf_kpdpgpu_block_size.png", width: 100%),
  caption: [Intervallo `dp_ms` per famiglia di peso e capacity, con mediana e intervallo interquartile su 21 repliche. Sopra: tempi del ciclo DP; sotto: rapporto fra il tempo mediano a 32 thread per blocco e quello agli altri block size.],
) <fig:kpdpgpu-block-size>

Nella campagna i valori mediani del wall sono centinaia di millisecondi, mentre `dp_ms` è generalmente di pochi millisecondi e arriva a qualche decina di millisecondi nelle configurazioni più lente a capacity $10^7$. Il wall è quindi prevalentemente costituito da operazioni esterne all'intervallo del ciclo DP, tra cui inizializzazione CUDA, allocazioni, trasferimenti, backtrack e deallocazioni. Queste attività non sono cronometrate separatamente, quindi non è possibile assegnare a ciascuna una quota del costo totale. L'intervallo `dp_ms` consente di confrontare le geometrie di lancio sul solo ciclo DP, ma non rappresenta il wall end-to-end.

Alle capacity $10^5$ e $10^6$ l'intervallo `dp_ms` mostra variazioni meno ampie in valore assoluto rispetto alla taglia $10^7$, senza essere perfettamente costante al variare del blocco. A capacity $10^7$ il tempo scende fortemente passando da 32 a 256 thread per blocco. Le differenze fra le configurazioni oltre 256 thread sono invece molto più contenute rispetto a quelle osservate fra 32 e 256 thread, e il minimo puntuale dipende dalla replica e dal seed. Le bande di @fig:kpdpgpu-block-size rappresentano l'intervallo interquartile delle 21 esecuzioni per configurazione. Queste misure non identificano la causa degli andamenti, perché non separano overhead dei lanci, occupazione e scheduling. Inoltre, nel modello di esecuzione CUDA non esistono barriere implicite fra warp di blocchi diversi all'interno dello stesso kernel, che non possono quindi spiegare l'andamento osservato.

=== Robustezza rispetto al seed

#figure(
  image("../images/perf_kpdpgpu_seed9.png", width: 100%),
  caption: [Tempo del ciclo DP `dp_ms` a capacity $10^7$. Per ciascun punto di blocco sono mostrate le mediane delle cinque repliche associate a nove seed; la linea rappresenta la mediana fra quelle mediane per seed. Il seed 42 è evidenziato in arancione.],
) <fig:kpdpgpu-seed9>

La campagna a seed variabile consente di distinguere la ripetibilità sullo stesso seed dalla sensibilità all'istanza. In entrambe le famiglie di peso, i confronti 32–256 thread conservano una differenza ampia e dello stesso segno in tutti i seed: cambiare la geometria modifica fortemente la durata di `dp_ms`. Al contrario, 256 e 1024 thread mostrano tempi vicini e le differenze per seed cambiano di segno in vari casi, quindi non si dimostra un peggioramento sistematico oltre 256 thread. Il grafico @fig:kpdpgpu-block-size e @fig:kpdpgpu-seed9 riportano esclusivamente la durata del ciclo DP misurata sull'host, non una stima dei tempi dei singoli kernel né un effetto diretto sul wall end-to-end.

=== Confronto con la versione CPU



Il confronto con il CPU classico usa le stesse istanze e lo stesso seed ed è limitato alle configurazioni campionate. Il wall end-to-end della GPU è circa $0.30$ s a capacity $10^6$, contro circa $0.029$ s del run CPU seriale e circa $0.033$ s di quello a 32 thread. A capacity $10^7$ i migliori wall GPU sono vicini al run CPU seriale, intorno a $0.295$ s, mentre il wall del CPU a 32 thread resta di circa $0.083$ s. Nelle configurazioni misurate non emerge quindi un vantaggio end-to-end della GPU; questo risultato non viene generalizzato ad altre configurazioni o a un confronto universale fra GPU e CPU.

Il confronto fra le fasi di calcolo usa l'intervallo host `dp_ms` per la GPU e la fase `compute` registrata dai timer del CPU: non si tratta quindi di due tempi puri dei rispettivi kernel hardware misurati con la stessa modalità. Nei valori relativi a `maxWeight=10`, a capacity $10^6$ la configurazione GPU con mediana minima di `dp_ms` impiega circa $3.61$ ms, meno dei circa $15$ ms del compute CPU seriale, ma più dei circa $2.4$ ms del compute CPU a 32 thread. A capacity $10^7$, invece, la configurazione GPU migliore impiega circa $6.85$ ms, meno del compute sia seriale, circa $167$ ms, sia a 32 thread, circa $22.2$ ms. Il vantaggio del ciclo DP GPU rispetto al compute seriale non implica quindi un vantaggio end-to-end, come mostra il confronto dei wall.
