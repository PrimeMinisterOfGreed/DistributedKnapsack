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

Il kernel è la traduzione più diretta possibile della frontiera classica,  la riga precedente viene letta direttamente dalla memoria globale. Questo rende l'algoritmo, come nella versione CPU, memory bound per costruzione: per ogni cella si leggono due valori e se ne scrive uno, mentre la parte aritmetica si riduce a una comparazione e una addizione.

La sincronizzazione è garantita implicitamente dal modello di esecuzione degli stream: i kernel consecutivi lanciati sullo stesso stream vengono eseguiti in ordine, quindi il kernel della riga $i$ vede la riga $i-1$ completa prima di partire. La dimensione del blocco è configurable mediante la variabile d'ambiente `KP_GPU_THREADS` (default 256) e viene limitata al massimo supportato dal device (`cudaDevAttrMaxThreadsPerBlock`).


== Knapsack DP GPU: Risultati

=== Esecuzione dell'esperimento

La campagna di misure è stata eseguita sul cluster mercurio con lo script Slurm riportato di seguito: per ogni combinazione di famiglia di peso ($"maxWeight" in {10, 1000}$), thread per blocco ($"KP_GPU_THREADS" in {32, 64, 128, 256, 512, 1024}$) e capacity ($10^5$, $10^6$, $10^7$) vengono ripetute 3 misure; per ogni replica lo script raccoglie dallo stdout di `kpbench` sia il tempo di esecuzione (wall time) del solver sia il tempo del solo ciclo di kernel DP, apponendoli a un CSV di riepilogo. La partizione espone una NVIDIA A100-PCIE-40GB (40 GB di memoria HBM2e, banda di picco dichiarata $approx 1.55" TB"\/"s"$). Si noti che la griglia contiene sempre $ceil((C+1)/t)$ blocchi, cioè un thread per cella: il parallelismo totale è fisso, il numero di thread per blocco cambia solo la geometria di lancio e l'occupazione delle SM. Le taglie in $"KP_GPU_THREADS"$ seguono proprio la struttura dell'hardware: i thread di un blocco vengono raggruppati ed emessi in warp di 32 lane, l'unità di schedulazione dell'SM, quindi si considerano solo multipli di 32; una taglia non allineata genererebbe un ultimo warp parzialmente vuoto che consuma comunque uno slot di schedulazione.

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
  caption: [Tempo del ciclo di kernel DP (sopra) e rapporto di tempo rispetto alla configurazione a 32 thread per blocco (sotto) #footnote[al variare dei thread per blocco, per famiglia di peso e capacity. Il parallelismo totale è fisso (un thread per cella): il rapporto misura l'effetto della geometria di lancio, non dello scaling.]],
) <fig:kpdpgpu-block-size>

La prima osservazione riguarda il tempo di esecuzione: a prescindere da capacity e block size il solver completa in tempi essenzialmente costanti al variare dei parametri. Questo tempo è dominato dal setup CUDA (creazione del contesto, allocazione della tabella, trasferimenti e backtrack), che viene pagato una volta per esecuzione; la metrica significativa è quindi il tempo del ciclo di kernel `dp_ms`.

A capacity piccola e media il tempo del kernel resta pressoché piatto al variare della taglia del blocco: il run è in un regime dominato dall'overhead dei $n$ lanci sequenziali e dalla sincronizzazione, che maschera qualunque effetto della geometria. L'effetto del block size emerge solo a capacity $10^7$, dove il lavoro per lancio supera l'overhead: come si vede in @fig:kpdpgpu-block-size, i blocchi piccoli pagano un tempo molto superiore a quelli grandi, e il tempo scende fino alla configurazione a 256 thread per blocco per poi risalire leggermente. La ragione del vantaggio dei blocchi più grandi è nell'occupazione delle SM: con un thread per cella e un numero di blocchi comunque molto elevato, blocchi più popolosi riducono il numero di blocchi da schedulare per lancio e le barriere implicite tra warp di blocchi diversi, mentre la curva che risale oltre 256 riflette il crollo del numero di blocchi, che a parità di warp inizia a togliere flessibilità allo scheduler delle SM nella copertura dei tempi di memoria. Il piccolo vantaggio dei blocchi a 32 thread alle taglie basse resta un effetto di margine: a quelle taglie entrambe le configurazioni vivono sotto l'overhead dei lanci.

=== Confronto con la versione CPU



Il confronto con la versione CPU classica (stesse istanze, stesso seed) va letto su due piani, e cambia profondamente dopo la correzione della versione CPU: la tabella row-major ha ridotto i tempi seriali di circa un ordine di grandezza, quindi il setup CUDA non è quasi più ammortizzabile. Sul tempo end-to-end la GPU non vince più in nessuna configurazione: l'overhead di setup — contesto, allocazione e trasferimenti, dell'ordine di un intero run CPU seriale alla taglia grande — si paga a ogni esecuzione a prescindere dal calcolo, quindi a capacity $10^7$ la GPU al più sfiora la parità col seriale CPU, resta chiaramente più lenta del run a 32 thread, e a capacity $10^6$ perde contro entrambi con larghissimo margine.

Sul solo tempo di calcolo il confronto usa la stessa metrica del capitolo DAG, la fase compute del CPU isolata dai timer: qui il kernel GPU batte nettamente il calcolo seriale CPU a entrambe le taglie — il gap si allarga con la capacity, perché il kernel mantiene il suo tempo quasi costante mentre il compute CPU cresce — ma contro il CPU a 32 thread il quadro si ribalta: a capacity $10^6$ il compute CPU è più veloce di qualunque configurazione del kernel GPU, e a $10^7$ il vantaggio residuo della GPU è ampio ma non più schiacciante. La conclusione va dunque ribaltata rispetto alla campagna precedente: il kernel GPU supera il calcolo seriale CPU, ma non il compute del CPU a 32 thread alla taglia media, e alla taglia grande il vantaggio residuo non basta ad ammortizzare il setup per istanze singole — end-to-end la GPU non vince mai. Il workload resta memory-bound sul device, ma la versione GPU diventa attraente solo quando il setup si ripaga su molte istanze (batch di solve), non sul singolo run.
