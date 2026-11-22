#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

#show: style-algorithm
= Knapsack DP DAG

Un modello alternativo di esecuzione al knapsack classico è pensare di creare un Directed Acyclic Graph che rappresenti il flusso di esecuzione invece di operare direttamente sulla linea come frontiera. Per farlo si consideri la ricorrenza $"Dp"_i(w)=max("Dp"_(i-1)(w), "Dp"_(i-1)(w-w_i)+v_i)$, allora si può raggiungere uno stato $(i,w)$ da $(i-1,w)$ e da $(i-1,w-w_i)$, questo insieme di archi impone sostanzialmente l'ordine di esecuzione dell'algoritmo sulle diverse celle della DP e dipende solo da $w_i$. A questo punto si possono definire le frontiere di questo DAG, in modo semplice, come l'insieme degli stati che possono essere calcolati in parallelo, dato che non dipendono da altri stati, formalmente dati due stati $u,v in F$ allora $u arrow.not v and v arrow.not u$ che formano quindi una anti chain.

Costruendo una frontiera molto semplice inizialmente, ad esempio $F_0={(0,0),(0,1),...,(0,W)}$ si possono costruire le frontiere successive utilizzando i successori dei nodi calcolati, dato un insieme di nodi completati $C$ questo insieme $"Ready"(C)={v in.not C : "Pred"(v) subset.eq C}$ forma la frontiera di nodi successiva. Se si suppone un parallelismo a livello di cella si ottiene che si sta riproducendo il flusso dell'algoritmo classico, infatti $F_1$ sarà $F_1={(1,0),(1,1),...,(1,W)}$.

Per generalizzare il comportamento delle frontiere si può considerare un parallelismo con granularità più grossolana della cella, per farlo si può dividere la DP a blocchi (o tile). Partendo dal caso più semplice si può pensare di raggruppare porzioni della prima riga, che ospita sostanzialmente tutti i nodi della frontiera iniziale in blocchi, ora ripetendo il ragionamento precedente si può costruire nuovamente la frontiera iniziale cambiando però la condizione, per ogni blocco $B_(i,j)$ se esiste una cella $w in B_(i,j)$ che ha come dipendenza una cella di un altro blocco allora il blocco è dipendente da quel blocco. Per aumentare ulteriormente la granularità e incorporare anche gli item (quindi le righe) nel tiling si può procedere estendo la formulazione considerando una dipendenza più generica: " se i predecessori di un elemento x appartengono a un altro tile, allora il blocco attuale è dipendente dal precedente". Supponendo di costruire il DAG con i tile $G(V,E)$ con $V={(i,w)}$ e $(x,y) in E$ allora dati due blocchi A e B $A arrow B arrow.l arrow "Pred"(B) inter A eq.not emptyset$ con $"Pred"(B)=union_(x in B) "Pred"(x)$. Una volta costruito il grafo si assegnano dei livelli ad ogni tile in base alla dipendenze, ognuno di questi livelli costituisce una frontiera di esecuzione di questo DAG i cui elementi si possono risolvere in parallelo, ad ogni elemento viene poi applicata la ricorrenza originale che risolve il problema dello zaino. L'algoritmo completo viene riassunto di seguito.

#KpDpDag

Per funzionare, la procedura presuppone che i vertici all'interno del grafo siano ordinati in ordine topologico, come si vedrà nell'algoritmo successivo questa condizione viene raggiunta implicitamente inserendo i vertici in ordine row-major. L'algoritmo si articola nelle seguenti fasi: 1) Creazione del DAG. 2) Assegnamento del livello ai vertici, che corrisponde alla loro frontiera di esecuzione. 3) Esecuzione in parallelo di ogni frontiera e ritorno del risultato. La generazione del DAG segue tutte le regole espresse precedentemente e riassunte nella procedura di seguito.


#GenerateDAG

Questa procedura è piuttosto generale e dipende fortemente da come si definisce formalmente il predecessore di una cella, nel caso del knapsack una cella può avere due predecessori: $"DP"[i-1][w]$ in caso si decida di non prendere l'elemento e $"DP"[i-1][w-w_i]$ nel caso in cui invece si decida di prenderlo. Per un certo elemento $i$ e un certo range di capacità $(c,c+g)$ dove g rappresenta quante celle sulla linea si vogliono raggruppare nel tile i predecessori sono due range $"DP"[i-1][c-w_i...c-w_i+g]$ e $"DP"[i-1][c...c+g]$ che rappresentano le due casistiche, a questo punto per ordinare l'esecuzione dei tile basta controllare quale dei tile contiene il range e aggiungere un arco tra quel blocco e quello corrente, si noti che i vertici vengono aggiunti in ordine row-major, questo garantisce che l'ordinamento topologico sia rispettato senza dover ricorrere a un operazione di ordinamento topologico esplicita. L'assegnamento dei livelli avviene utilizzando questa definizione in modo analogo
#AssignLevels



Questa piccola funzione implementa sostanzialmente questa ricorrenza $L(v)= cases(0 "if" "Pred"(v) = emptyset, 1+max_(x."level" in "Pred"(v)))$ che prende il massimo livello tra tutti gli elementi dei blocchi predecessori. Questa parte è piuttosto critica, siccome il livello dei predecessori deve essere assegnato prima del blocco corrente i vertici devono essere esplorati in ordine topologico, tuttavia per le condizioni precedenti questa condizione ci viene garantita dalla fase di costruzione stessa del DAG. A questo punto manca solo l'ultima fase di calcolo della funzione nei vari tile

#ComputeTile

Per ogni tile si procede con il consueto calcolo sulla DP e si ricava il risultato.

== Knapsack DP DAG: implementazione

Nell'implementazione i tile sono i vertici di un grafo `boost::adjacency_list` bidirezionale e ogni vertice porta appeso un `NodeData` che descrive il sottoblocco della tabella di sua competenza. Il tiling è parametrizzato da `item_block` e `cap_block`, che accorpano rispettivamente le righe (gli item) e le colonne (le capacità) della DP: la griglia ha $n_b = ceil(n\/"item"_b)$ blocchi di item e $n_q = ceil((C+1)\/"cap"_b)$ blocchi di capacità, e poiché i vertici vengono inseriti in ordine row-major il descrittore del tile $(b, q)$ è semplicemente $b dot n_q + q$. Il blocco DP è una matrice Eigen row-major con una riga in più rispetto agli item locali del tile: la riga 0 non viene calcolata ma ereditata dal tile sopra (riga di confine), mentre per ogni item locale `dep_ranges` registra l'intervallo di capacità letto nel caso "take", cioè il range del tile shiftato di $w_i$.

#figure(
  caption: "Knapsack DP DAG: strutture dati del grafo",
  kind: "listing",
  sourcecode[
    ```cpp
    using BlockMatrix = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    struct NodeData
    {
        int b, q;          // coordinate del tile nella griglia
        int rows;          // item locali + 1 (riga 0 = riga di confine)
        int width;         // colonne di capacità del tile
        int level;         // livello = frontiera di esecuzione
        int lo, hi;        // intervallo di capacità posseduto
        std::vector<std::pair<int, int>> dep_ranges; // dipendenza per item locale
        BlockMatrix block; // blocco DP, dimensioni rows x width
    };

    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
        boost::property<boost::vertex_index_t, int, NodeData>>;
    ```
  ],
)



La costruzione del grafo avviene in due passaggi: prima si creano tutti i vertici, in modo che siano in ordine row-major calcolando gli intervalli di dipendenza al loro interno, successivamente si aggiungono gli archi, per farlo si prende un arco col tile direttamente sopra $(b-1,q)$ che rappresenta il caso "skip", poi per ogni item locale si aggiungono archi dai blocchi di capacità che intersecano l'intervallo spostato di $w_i$ , che può trovarsi o nella riga di tile sopra oppure a sinistra della stessa riga. L'intervallo di dipendenza di un item locale è l'intervallo di capacità che il caso "take" legge nella riga precedente: per ogni $w$ dell'intervallo del tile si legge $"DP"[i-1][w - w_i]$, quindi l'intervallo è quello del tile shiftato di $-w_i$ e troncato al dominio di capacità $[0, C]$, e viene marcato vuoto quando l'item non entra mai nell'intervallo del tile. Ogni arco rappresenta la regola: "se un predecessore di una cella del tile vive in un altro tile, il tile dipende da quel tile"; l'inserimento row-major dei vertici garantisce l'ordinamento topologico senza alcuna operazione esplicita, come richiesto dalla procedura.

L'assegnamento dei livelli (`compute_levels`) è un singolo passaggio sui vertici: quando un tile viene visitato tutti i suoi predecessori hanno descrittore minore per cui il livello è già assegnato. La risoluzione (`solve_dag`) smista i tile in livelli ed esegue le frontiere una dopo l'altra, con parallelismo interno al livello.

#figure(
  caption: "Knapsack DP DAG: esecuzione a frontiere",
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

La barriera implicita a fine `parallel for` realizza esattamente la semantica di $"Ready"(C)$ del modello: un tile del livello $L+1$ non comincia prima che tutti quelli del livello $L$ abbiano scritto il proprio blocco. Il valore ottimo si legge infine nell'angolo in basso a destra dell'ultimo tile, `block(rows - 1, capacity - lo)`.

Il calcolo di un singolo tile (`compute_tile`) è la ricorrenza classica confinata al sottoblocco. La riga 0 viene copiata dal tile sopra: per ogni capacità $w$ dell'intervallo si risale al tile proprietario ($q' = w\/"cap"_b$, colonna $w - q' dot "cap"_b$) e se ne legge l'ultima riga, oppure si scrivono zeri se $b = 0$. Le righe interne dividono la riga in tre intervalli contigui: prima che l'item entri ($"lo" + c < w_i$, copia della riga sopra), poi il caso "take" con sorgente $w - w_i$ in un tile a sinistra della stessa riga, infine il caso "take" con sorgente interna al tile, che è lo stesso kernel branch-free a puntatori delle altre varianti: i due estremi degli intervalli ($"lo" + c = w_i$ e $w - w_i = "lo"$) si calcolano una volta per riga, fuori dal ciclo caldo.

La funzione di alto livello `knapsackdpdag` incolla le fasi: costruzione del grafo, risoluzione, ricostruzione della soluzione e al termine registra in `DAGStats` il numero di tile ($n_b dot n_q$), di archi e di livelli, insieme alla larghezza delle frontiere (tile per livello, riassunta con media, mediana, minimo e massimo): sono le statistiche riportate in @tab:dag-frontiers nella sezione dei risultati.

== Knapsack DP DAG: Risultati

=== Esecuzione dell'esperimento

Le misure condividono l'ambiente del capitolo precedente (nodo ARM Neoverse-N1 del cluster mercurio, `kpbench` compilato con GCC 14.2.0 in preset Release, misure `perf stat`). Questo consente un confronto diretto fra le due versioni: tempi e contatori provengono dalla stessa campagna. In fase di comparazione non si usa il wall per comparare le soluzioni ma la sola fase di compute, questo perchè il DAG paga un tempo addizionale in termini di costruzione del grafo e di deallocazione dello stesso che la soluzione DP non ha. Costo che non dovrebbe essere pagato se per esempio si riciclasse il grafo cambiando solo i profitti e non i pesi degli elementi.

=== Scalabilità e crossover



#figure(
  image("../images/perf_kpdpdag_speedup_vs_n.png", width: 100%),
  caption: [Speedup della fase compute del KnapsackDPDAG al variare di thread e numero di oggetti (capacity $10^6$, granularità tile migliore per punto, mediana su 3 repliche).],
) <fig:dag-speedup>

La @tab:dag-campaigns riassume la configurazione della campagna di @fig:dag-speedup.

#figure(
  table(
    columns: 5,
    [$n$], [$C$], [$W_"max"$], [$"item"_b$ × $"cap"_b$], [thread (repliche)],
    [50], [$10^6$], [10], [(25, 10³) a 2-4-32 t, (10, 10³) a 8 t, (25, 10⁴) a 1 t], [1, 2, 4, 8, 16, 32 (3)],
    [1000], [$10^6$], [10], [(25, 10³) a 1-8 t, (10, 10⁴) a 16-32 t], [1, 4, 8, 16, 32 (3)],
    [5000], [$10^6$], [10], [(25, 10³) a 1 t, (25, 10⁴) a 4-32 t], [1, 4, 8, 16, 32 (3)],
  ),
  caption: [Configurazione della campagna compute: geometria migliore per punto (selezionata dalle campagne di wall), mediana su 3 repliche.],
) <tab:dag-campaigns>

La campagna riassunta in @tab:dag-campaigns fissa la capacity e varia il numero di oggetti. A $n = 50$ nemmeno la fase di compute trae vantaggio da 32 esecutori: la frontiera , che hanno al massimo 2 tile per livello, non permettono di aumentare il parallelismo a un livello sufficiente. Il limite si allenta col numero di oggetti, dove il compute scala quasi idealmente; i wall della stessa configurazione crescono però molto meno, diluiti da first touch e teardown (@tab:dag-phases), che a taglia grande coprono da soli la maggior parte del wall.

#figure(
  table(
    columns: 8,
    [$n$], [$"item"_b$], [$"cap"_b$], [tile], [livelli], [$"min"$], [$"mediana"$], [$"media"\/"max"$],
    [50], [5], [$10^2$], [100010], [10010], [1], [10.0], [9.99 \/ 10],
    [50], [5], [$10^3$], [10010], [1010], [1], [10.0], [9.91 \/ 10],
    [50], [5], [$10^4$], [1010], [110], [1], [9.9], [9.18 \/ 10],
    [50], [10], [$10^2$], [50005], [10005], [1], [5.0], [5.00 \/ 5],
    [50], [10], [$10^3$], [5005], [1005], [1], [5.0], [4.98 \/ 5],
    [50], [10], [$10^4$], [505], [105], [1], [5.0], [4.81 \/ 5],
    [50], [25], [$10^2$], [20002], [10002], [1], [2.0], [2.00 \/ 2],
    [50], [25], [$10^3$], [2002], [1002], [1], [2.0], [2.00 \/ 2],
    [50], [25], [$10^4$], [202], [102], [1], [2.0], [1.98 \/ 2],
    [5000], [5], [$10^2$], [10001000], [11000], [1], [993.2], [909.2 \/ 1000],
    [5000], [5], [$10^3$], [1001000], [2000], [1], [478.8], [500.5 \/ 1000],
    [5000], [5], [$10^4$], [101000], [1100], [1], [100.3], [91.8 \/ 101],
    [5000], [10], [$10^2$], [5000500], [10500], [1], [499.1], [476.2 \/ 500],
    [5000], [10], [$10^3$], [500500], [1500], [1], [368.4], [333.7 \/ 500],
    [5000], [10], [$10^4$], [50500], [600], [1], [97.9], [84.2 \/ 101],
    [5000], [25], [$10^2$], [2000200], [10200], [1], [199.9], [196.1 \/ 200],
    [5000], [25], [$10^3$], [200200], [1200], [1], [193.9], [166.8 \/ 200],
    [5000], [25], [$10^4$], [20200], [300], [1], [74.6], [67.3 \/ 101],
  ),
  caption: [Statistiche delle frontiere del DAG (larghezza = tile per livello)],
) <tab:dag-frontiers>

Cambiando invece la geometria dei tile si può notare invece che anche la fase di compute guadagna dal maggior numero di thread (linea verde della @fig:dag-speedup), in tabella @tab:dag-frontiers si può notare come variando opportunamente la geometria dei tile il parallelismo interno ai livelli aumenti considerevolmente. Inoltre la struttura del grafo rende la distribuzione deterministica: oltre all'arco col tile sopra $(b-1, q)$, il range di dipendenza shiftato di $w_i$ crea sistematicamente un arco col tile alla sua sinistra $(b, q-1)$, quindi il livello di un tile è $"level"(b,q) = b+q$ . Ne segue che i livelli sono $n_b + n_q - 1$, il picco di larghezza è $min(n_b, n_q)$ e la media $n_b n_q\/(n_b+n_q-1)$.



#figure(
  table(
    columns: 10,
    [$n$],
    [$W_"max"$],
    [$"item"_b$],
    [$"cap"_b$],
    [thread],
    [wall (ms)],
    [graph build (ms)],
    [first touch (ms)],
    [compute (ms)],
    [teardown (ms)],

    [50], [10], [25], [$10^4$], [1], [98.1], [0.3], [65.0], [12.7], [20.2],
    [50], [10], [25], [$10^3$], [32], [44.2], [3.1], [8.2], [13.3], [19.5],
    [1000], [10], [10], [$10^4$], [32], [464.8], [14.1], [134.4], [42.6], [273.0],
    [5000], [10], [25], [$10^4$], [32], [2050.1], [38.5], [621.9], [193.2], [1194.6],
  ),
  caption: [Fasi del solver DAG su 4 configurazioni (mediana di 3, geometria migliore per punto): a $n >= 1000$ il teardown copre da solo oltre metà del wall a 32 thread.],
) <tab:dag-phases>


Il peso delle fasi non compute è comunque strettamente correlato al numero di tile, la costruzione che è seriale per natura fa si che il costo cresca linearmente con il numero di tile, costo che potrebbe essere evitato se il calcolo riutilizzasse il grafo variando per esempio solo i profitti e mantenendo inalterati i pesi. La riga delle fasi di @fig:dag-block-grid mostra però il teardown come eccezione: resta quasi costante al variare della granularità, perché la memoria da liberare (la tabella DP) non dipende dalla forma dei tile; il suo peso nasce dall'anti-scaling: i blocchi nascono in arena separate dell'allocator e risultano sparsi in memoria, quindi la liberazione si esegue quasi tutta su un solo core, nel percorso di free del kernel. Confinare l'allocator a un'arena sola riporterebbe il teardown al costo seriale, ma renderebbe il first touch molto più lento, privo delle transparent huge pages.





// aggiungi forse un grafico sul wall



=== L'effetto del tiling sulla località

#figure(
  image("../images/perf_vs_algorithms.png", width: 100%),
  caption: [Confronto KnapsackDP classico e KnapsackDPDAG a capacity $10^6$ e $n=50$],
) <fig:dag-vs-classic>

La @fig:dag-vs-classic confronta i contatori hardware dei due algoritmi sull'intero processo, a parità di istanza ($n = 50$, capacity $10^6$) e per entrambe le famiglie di peso. In seriale i due profili sono simili: il DAG mostra un IPC leggermente più basso del classico e un numero di cicli per cella più alto, mentre le miss di ultimo livello per istruzione restano confrontabili fra i due. Al crescere dei thread i comportamenti divergono: l'IPC del classico crolla progressivamente, scendendo a circa $0.25$ a 32 thread, e i cicli per cella crescono di oltre un ordine di grandezza rispetto al seriale, mentre il DAG mantiene un IPC elevato — nella famiglia $W_"max" = 10$ cresce fino a circa $2.3$ — e i suoi cicli per cella aumentano in modo molto più contenuto. Anche il rapporto fra le miss di ultimo livello si rovescia: a 8 thread le miss per istruzione scendono per entrambi gli algoritmi, ma a 32 thread quelle del classico risalgono verso i $9$-$10$ MPKI mentre quelle del DAG scendono a circa $2$. Il fenomeno non dipende dalla famiglia di peso: le due righe della figura presentano la stessa dinamica, con il vantaggio del DAG sui contatori che emerge solo nel regime parallelo.


=== La granularità dei tile

#figure(
  image("../images/perf_kpdpdag_block_grid.png", width: 100%),
  caption: [Effetto di item_block e cap_block a $n = 5000$, capacity $10^6$, 32 thread. Sopra: wall, IPC e LLC MPKI. Sotto: tempi di first touch, compute e teardown (mediana di 3, scala logaritmica condivisa fra i pannelli).],
) <fig:dag-block-grid>

La @fig:dag-block-grid mostra come la geometria dei tile incida sulle fasi del solver a $n = 5000$ e 32 thread. Il primo aspetto evidente è che il wall time non è dominato dal calcolo: la fase di compute occupa solo $170$-$210$ ms, meno di un decimo del wall, mentre first touch e teardown assorbono insieme la grande maggioranza del tempo, con il teardown che pesa da solo per circa metà del wall. Il secondo aspetto è che queste fasi reagiscono in modo diverso alla granularità: il teardown resta quasi costante in tutte le configurazioni ($1.2$-$1.4$ s), confermando che la memoria da liberare dipende dalla tabella DP e non dalla forma dei tile. Il compute varia poco e non in modo monotono: il minimo si ottiene a $(25, 10^3)$, ma la configurazione con il wall migliore è $(25, 10^4)$, segno che a questa taglia la scelta della geometria premia le fasi di sistema più che il kernel. Coerentemente, i contatori cambiano poco fra le quattro configurazioni : l'IPC resta compreso fra $1.32$ e $1.42$ e le miss di ultimo livello fra $10.4$ e $11.9$ MPKI , ad indicare che il tiling agisce soprattutto sul costo della gestione della memoria, senza alterare in modo marcato il comportamento del processore durante il calcolo.

== Confronto tra la versione classica e la versione DAG

#figure(
  image("../images/perf_big_crossover.png", width: 100%),
  caption: [Tempi di compute $T_"plain"\/T_"dag"$ a capacity $10^6$ ($> 1$: DAG più veloce).],
) <fig:kpdp-perf-crossover>

Nella sola fase di *compute*, la @fig:kpdp-perf-crossover mostra che a $n = 50$ il DAG è più rapido soltanto in seriale ($1.18 times$): all'aumentare dei thread il rapporto scende fino a $0.18$ a 32 thread, poiché la frontiera contiene al massimo due tile e il kernel usa effettivamente due thread. Per $n = 1000$ e $n = 5000$ il classico resta più veloce nel calcolo, ma il DAG riduce lo svantaggio, arrivando rispettivamente a $0.78$ e $0.84$ a 32 thread grazie alle frontiere più ampie. Il risultato si ribalta nella @fig:dag-crossover-wall, dove il tempo comprende anche la costruzione del grafo, il first touch e il teardown: per $n >= 1000$ a 32 thread il minor costo complessivo delle fasi fuori dal compute compensa il kernel ancora più lento, portando il rapporto del wall sopra 1. A $n = 50$, invece, il rapporto resta sotto 1 perché prevale il forte svantaggio del compute.

#figure(
  table(
    columns: 5,
    [$n$], [$T_"comp"$ plain(1)], [$T_"comp"$ dag(1)], [$T_"comp"(1)$ plain\/DAG], [$T_"comp"(32)$ plain\/DAG],

    [50], [15.0 ms], [12.7 ms], [$1.18 times$], [$0.18 times$],
    [1000], [298 ms], [428 ms], [$0.69 times$], [$0.78 times$],
    [5000], [1.49 s], [2.18 s], [$0.68 times$], [$0.84 times$],
  ),
  caption: [Crossover della fase compute: tempi seriali della fase compute e rapporto fra i compute dei due solver in seriale e a 32 thread.],
) <tab:dag-crossover>

#figure(
  image("../images/perf_big_crossover_wall.png", width: 100%),
  caption: [Tempi di wall $T_"plain"\/T_"dag"$ a capacity $10^6$ ($> 1$: DAG più veloce).],
) <fig:dag-crossover-wall>

Nel confronto sui tempi di wall, mostrato nella @fig:dag-crossover-wall, il quadro cambia rispetto al solo compute. A $n = 50$ il classico rimane più veloce a ogni numero di thread, sebbene il divario si riduca fino a un rapporto di circa $0.68$ a 32 thread. Per $n = 1000$ e $n = 5000$, invece, il DAG recupera progressivamente lo svantaggio seriale e supera il classico a 32 thread, con rapporti rispettivamente di circa $1.20$ e $1.40$. Il vantaggio del DAG sul tempo complessivo compare quindi solo a taglia grande e con parallelismo elevato, mentre il classico resta più rapido in seriale e, considerando il solo kernel, anche a 32 thread.

#figure(
  image("../images/perf_big_comparison.png", width: 100%),
  caption: [Confronto KnapsackDP classico e KnapsackDPDAG (geometria migliore per punto) su $n = 5000$, capacity $10^6$, maxWeight $10$ ],
) <fig:dag-big-comparison>

La @fig:dag-big-comparison, relativa a $n = 5000$, mostra come i contatori hardware dei due algoritmi evolvano all'aumentare dei thread. In seriale il DAG presenta un IPC leggermente inferiore e molti più cicli per cella rispetto al classico, coerentemente con il suo maggiore tempo di compute. Parallelizzando, i comportamenti si separano nettamente: per il classico l'IPC scende da circa $1.8$ a $0.26$ a 32 thread, mentre i cicli per cella crescono da meno di $2$ a oltre $50$; per il DAG l'IPC resta elevato, circa $1.36$ a 32 thread, e i cicli per cella aumentano solo da circa $7$ a $9$, restando oltre cinque volte inferiori a quelli del classico a parità di thread. Le miss di ultimo livello per istruzione mostrano invece un andamento diverso: il DAG parte con meno MPKI in seriale, ma a 16 e 32 thread è il classico a registrarne meno, mentre i valori del DAG diminuiscono solo lievemente. Poiché i contatori riguardano l'intero processo e non il solo kernel, questo non contraddice il fatto che a 32 thread il classico resti più rapido nel compute, pur risultando più lento sul wall.
