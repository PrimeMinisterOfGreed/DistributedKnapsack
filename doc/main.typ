#import "functions/preamble.typ": *
#show: style-doc

#align(center)[
  #text(size: 17pt, weight: "bold")[Knapsack parallelo: implementazione, comparazione e analisi]
  #v(0.4em)
  #text(size: 12pt)[Matteo Ielacqua]
  #v(1em)
]

= Sommario
Il documento presenta tre implementazioni parallele dell'algoritmo dello zaino basato su programmazione dinamica (DP). La versione classica parallelizza la riga corrente della tabella. La seconda implementazione rappresenta le dipendenze con un grafo aciclico orientato (DAG) i cui vertici sono tile della tabella, raggruppati in livelli eseguiti in sequenza e parallelizzati al loro interno. L'ultima parte presenta l'implementazione CUDA per GPU NVIDIA.

#include "chapters/knapsack_dp_classic.typ"

#include "chapters/knapsack_dp_dag.typ"

#include "chapters/knapsack_dp_gpu.typ"

= Conclusioni

La campagna estesa consente di distinguere quali conclusioni sono supportate dalla multiplicità delle esecuzioni e della variazione del seed. La @tab:evidence-bounds riassume l'ambito delle principali evidenze, indicando anche come queste non devono essere interpretate oltre le configurazioni misurate.

#figure(
  table(
    columns: 3,
    [Aspetto], [Evidenza], [Confine della conclusione],
    [GPU, 32 → 256 thread], [Il tempo del ciclo DP scende fortemente a capacità $10^7$, in entrambe le famiglie di pesi e in tutti i seed misurati.], [Il risultato non dimostra quali meccanismi determinino questo beneficio, come occupancy, DRAM o scheduling.],
    [GPU, 256 → 1024 thread], [I tempi restano vicini e le differenze per seed cambiano segno in vari casi.], [Non è dimostrata una regressione uniforme oltre 256 thread né un optimum universale del block size.],
    [DAG, 8 → 32 thread], [Il vantaggio addizionale è limitato e varia con la geometria, circa $1.09$–$1.21$ volte per seed in alcuni casi a $n=5000$.], [Il wall time non coincide con lo scaling ideale e non attribuisce l'effetto a un unico meccanismo.],
    [`compute` e wall time], [Un solver con una fase `compute` più lenta può avere un wall time inferiore.], [Ogni confronto deve precisare quale fase considera: `compute`, `first_touch`, `graph_build`, `teardown` o wall time.],
  ),
  caption: [Evidenze robuste e corrispettivo confine inferenziale],
) <tab:evidence-bounds>

== Validità e riproducibilità

Le conclusioni valgono per il nodo Neoverse-N1 e per taglie, capacità, geometrie e famiglie di pesi analizzate. I nove seed descrivono le istanze campionate, non qualunque input; un optimum locale del DAG o della GPU non è quindi universale. Le figure basate su `dp_ms` misurano sull'host l'intero ciclo DP GPU, mentre i confronti fra `compute` e wall time usano soltanto campagne con timer di fase.

Le misure sono riproducibili rilanciando gli script `sbatch` del repository; i grafici statistici si rigenerano con `python3 doc/kpdp_statistical_plots.py`.

#bibliography("biblio.bib", style: "ieee", title: [Bibliografia])
