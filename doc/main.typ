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

La campagna estesa consente di distinguere quali conclusioni sono supportate dalla multiplicità delle esecuzioni e della variazione del seed. Le misure supportano quattro conclusioni circoscritte. A capacità $10^7$, il passaggio da 32 a 256 thread per blocco riduce marcatamente `dp_ms` in entrambe le famiglie di pesi e in tutti i seed misurati. Oltre 256 thread le differenze sono più contenute e cambiano segno in vari casi: non dimostrano né una regressione uniforme né un optimum universale del block size, né identificano i meccanismi responsabili degli andamenti. Nel DAG, il vantaggio addizionale fra 8 e 32 thread è limitato e dipende dalla geometria, con rapporti di circa $1.09$–$1.21$ in alcuni casi a $n=5000$; non equivale a uno scaling ideale e non identifica un unico meccanismo. Infine, un solver con una fase `compute` più lenta può avere un wall time inferiore: ogni confronto deve quindi precisare la fase considerata, fra `compute`, `first_touch`, `graph_build`, `teardown` e wall time.

== Validità e riproducibilità

Le conclusioni valgono per il nodo Neoverse-N1 e per taglie, capacità, geometrie e famiglie di pesi analizzate. I nove seed descrivono le istanze campionate, non qualunque input; un optimum locale del DAG o della GPU non è quindi universale. Le figure basate su `dp_ms` misurano sull'host l'intero ciclo DP GPU, mentre i confronti fra `compute` e wall time usano soltanto campagne con timer di fase.

Le misure sono riproducibili rilanciando gli script `sbatch` del repository; i grafici statistici si rigenerano con `python3 doc/kpdp_statistical_plots.py`.

#bibliography("biblio.bib", style: "ieee", title: [Bibliografia])
