#import "../functions/preamble.typ": *

#show: style-algorithm
= DP su GPU

L'implementazione CUDA per GPU NVIDIA dell'algoritmo dello zaino basato su programmazione dinamica riprende la frontiera di esecuzione del capitolo classico: la pseudocodifica è infatti la stessa presentata in @algo-kpdp, cambia sostanzialmente la macchina su cui la frontiera viene eseguita. Invece di un insieme di thread della CPU che collaborano su una riga della DP, ogni cella della riga corrente viene assegnata a un thread del grid CUDA; l'accesso agli elementi $w$ della riga è contiguo, quindi i thread di uno stesso warp effettuano accessi alla memoria globale e la ricorrenza non richiede strutture di supporto aggiuntive.

== Implementazione

Il cuore dell'implementazione è il kernel che calcola una riga della DP, ogni thread calcola esattamente una cella.

#figure(
  caption: "Kernel CUDA per una riga della DP",
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
  caption: "Struttura del ciclo DP GPU e del suo intervallo di misura",
  kind: "listing",
  sourcecode[
    ```cpp
    cudaMalloc(&d_dp, tableSize * sizeof(int));
    cudaMemset(d_dp, 0, cols * sizeof(int)); // soltanto la riga iniziale
    cudaDeviceSynchronize();

    auto dpStart = std::chrono::steady_clock::now();
    for (int i = 1; i <= n; ++i) {
        const int *prev = d_dp + (i - 1) * cols;
        int *cur = d_dp + i * cols;
        knapsack_step_kernel<<<blocks, threads>>>(
            prev, cur, weights[i - 1], values[i - 1], cols);
    }
    cudaDeviceSynchronize();
    auto dpEnd = std::chrono::steady_clock::now();
    ```
  ],
)

Il kernel è la traduzione più diretta possibile della frontiera classica e la riga precedente viene letta direttamente dalla memoria globale. Per ogni cella il kernel CUDA legge `prev[w]` e, quando l'oggetto è ammissibile, legge anche `prev[w-weight]` sommandovi il profitto dell'oggetto; infine scrive `cur[w]`. Il confronto fra i due valori e l'eventuale addizione comportano poche operazioni aritmetiche per accesso in memoria. Le misure della campagna non riportano però la banda effettiva o altri contatori specifici dei sottosistemi di memoria, e non consentono quindi di classificare con certezza il kernel come memory bound.

La sincronizzazione è garantita implicitamente dal modello di esecuzione degli stream: i kernel consecutivi lanciati sullo stesso stream vengono eseguiti in ordine, quindi il kernel della riga $i$ vede la riga $i-1$ completa prima di partire. La dimensione del blocco è configurabile mediante la variabile d'ambiente `KP_GPU_THREADS` (default 256) e viene limitata al massimo supportato dal device (`cudaDevAttrMaxThreadsPerBlock`) @cudaCppGuide.


== Risultati

=== Esecuzione dell'esperimento

Gli `sbatch` del repository eseguono le misure su una NVIDIA A100-PCIE-40GB, variando la famiglia di pesi ($"maxWeight" in {10, 1000}$), i thread per blocco ($"KP_GPU_THREADS" in {32, 64, 128, 256, 512, 1024}$) e la capacità ($10^5$, $10^6$, $10^7$). Ogni esecuzione registra il wall time `time_s` e l'intervallo `dp_ms`; il campionamento è riassunto in @tab:misure-campionamento. La griglia contiene sempre $ceil((C+1)/t)$ blocchi: variare $t$ cambia la geometria di lancio, non il numero complessivo di celle elaborate. Si considerano multipli di 32 perché i thread vengono schedulati in warp di 32 lane.

`dp_ms` è misurato sull'host dall'inizio del ciclo dei lanci alla sincronizzazione finale: comprende lanci, esecuzione sul device e sincronizzazione, non il solo tempo dei kernel CUDA. Sono esclusi allocazione e `cudaMemset` iniziali, oltre ai successivi trasferimenti, alla ricostruzione della soluzione e alle deallocazioni.

=== Effetto dei thread per blocco

#figure(
  image("../images/perf_kpdpgpu_block_size.png", width: 100%),
  caption: [Intervallo `dp_ms` per famiglia di pesi e capacità (mediana su 21 ripetizioni). Sopra: tempi del ciclo DP GPU; sotto: rapporto rispetto a 32 thread per blocco.],
) <fig:kpdpgpu-block-size>

Nella campagna i valori mediani del wall time sono centinaia di millisecondi, mentre `dp_ms` è generalmente di pochi millisecondi e arriva a qualche decina di millisecondi nelle configurazioni più lente a capacità $10^7$. Il wall time è quindi prevalentemente costituto da operazioni esterne al ciclo DP GPU, tra cui inizializzazione CUDA, allocazioni, trasferimenti, ricostruzione della soluzione e deallocazioni. Queste attività non sono cronometrate separatamente, quindi non è possibile assegnare a ciascuna una quota del costo totale. `dp_ms` consente di confrontare le geometrie di lancio sul solo ciclo DP GPU, ma non rappresenta il wall time del solver.

Alle capacità $10^5$ e $10^6$, `dp_ms` mostra variazioni meno ampie in valore assoluto rispetto alla capacità $10^7$, senza essere perfettamente costante al variare del blocco. A capacità $10^7$ il tempo scende fortemente passando da 32 a 256 thread per blocco. Le differenze fra le configurazioni oltre 256 thread sono invece molto più contenute rispetto a quelle osservate fra 32 e 256 thread, e il minimo puntuale dipende dalla ripetizione e dal seed. Queste misure non identificano la causa degli andamenti, perché non separano overhead dei lanci, occupazione e scheduling. Inoltre, nel modello di esecuzione CUDA non esistono barriere implicite fra warp di blocchi diversi all'interno dello stesso kernel, che non possono quindi spiegare l'andamento osservato.

=== Robustezza rispetto al seed

#figure(
  image("../images/perf_kpdpgpu_seed9.png", width: 100%),
  caption: [Intervallo `dp_ms` a capacità $10^7$ per nove seed: punti = mediane di 5 ripetizioni; linea = mediana delle mediane; seed 42 in arancione.],
) <fig:kpdpgpu-seed9>

La campagna a seed variabile consente di distinguere la ripetibilità sullo stesso seed dalla sensibilità all'istanza. In entrambe le famiglie di pesi, i confronti 32–256 thread conservano una differenza ampia e dello stesso segno in tutti i seed: cambiare la geometria modifica fortemente la durata di `dp_ms`. Al contrario, 256 e 1024 thread mostrano tempi vicini e le differenze per seed cambiano di segno in vari casi, quindi non si dimostra un peggioramento sistematico oltre 256 thread. I grafici @fig:kpdpgpu-block-size e @fig:kpdpgpu-seed9 riportano esclusivamente la durata del ciclo DP GPU misurata sull'host, non una stima dei tempi dei singoli kernel CUDA né un effetto diretto sul wall time del solver.

=== Confronto con il solver classico CPU



Il confronto con il solver classico CPU usa le stesse istanze e lo stesso seed ed è limitato alle configurazioni campionate. Il wall time della GPU è circa $0.30$ s a capacità $10^6$, contro circa $0.029$ s del run CPU seriale e circa $0.033$ s di quello a 32 thread. A capacità $10^7$ i migliori wall time GPU sono vicini al run CPU seriale, intorno a $0.295$ s, mentre il wall time della CPU a 32 thread resta di circa $0.083$ s. Nelle configurazioni misurate non emerge quindi un vantaggio della GPU sul wall time; il risultato non viene generalizzato ad altre configurazioni.

Il confronto fra le fasi di calcolo usa l'intervallo host `dp_ms` per la GPU e `compute` registrato dai timer della CPU: non sono misure omogenee dei rispettivi dispositivi. Con `maxWeight=10`, a capacità $10^6$ la configurazione GPU con mediana minima di `dp_ms` impiega circa $3.61$ ms, meno dei circa $15$ ms di `compute` del solver classico seriale, ma più dei circa $2.4$ ms a 32 thread. A capacità $10^7$ la configurazione GPU migliore impiega circa $6.85$ ms, meno di `compute` sia seriale, circa $167$ ms, sia a 32 thread, circa $22.2$ ms. Il vantaggio del ciclo DP GPU rispetto a `compute` seriale non implica quindi un vantaggio sul wall time.
