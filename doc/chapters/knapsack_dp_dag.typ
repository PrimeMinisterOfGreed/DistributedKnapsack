#import "../functions/preamble.typ": *
#import "kp_dp_algo.typ": *

= Knapsack DP DAG: Modello

Un modello alternativo di esecuzione per questo algoritmo è pensare di creare un Directed Acyclic Graph che rappresenti la DP invece di usare una matrice. Per farlo si consideri la ricorrenza $"Dp"_i(w)=max("Dp"_(i-1)(w), "Dp"_(i-1)(w-w_i)+v_i)$, allora si può raggiungere uno stato $(i,w)$ da $(i-1,w)$ e da $(i-1,w-w_i)$, questo insieme di archi impone sostanzialmente l'ordine di esecuzione dell'algoritmo sulle diverse celle della DP e dipende solo da $w_i$. A questo punto si possono definire le frontiere di questo DAG, in modo semplice, come l'insieme degli stati che possono essere calcolati in parallelo, dato che non dipendono da altri stati, formalmente dati due stati $u,v in F$ allora $u arrow.not v and v arrow.not u$ che formano quindi una anti chain.

Costruendo una frontiera molto semplice inizialmente, ad esempio $F_0={(0,0),(0,1),...,(0,W)}$ si possono costruire le frontiere successive utilizzando i successori dei nodi calcolati, dato un insieme di nodi completati $C$ questo insieme $"Ready"(C)={v in.not C : "Pred"(v) subset.eq C}$ forma la frontiera di nodi successiva. Se si suppone un parallelismo a livello di cella si scopre in fretta che si riproduce praticamente l'esecuzione dell'algoritmo "naive", infatti $F_1$ sarà $F_1={(1,0),(1,1),...,(1,W)}$.

Per ottenere una soluzione diversa, computazionalmente parlando, è necessario considerare un parallelismo con granularità più grossolana della cella, per farlo si può dividere la DP a blocchi (o tile). Partendo dal caso più semplice si può pensare di raggruppare porzioni della prima riga, che ospita sostanzialmente tutti i nodi della frontiera iniziale in blocchi, ora ripetendo il ragionamento precedente si può costruire nuovamente la frontiera iniziale cambiando però la condizione, per ogni blocco $B_(i,j)$ se esiste una cella $w in B_(i,j)$ che ha come dipendenza una cella di un altro blocco allora il blocco è dipendente da quel blocco. Per aumentare ulteriormente la granularità e incorporare anche gli item (quindi le righe) nel tiling si può procedere estendo la formulazione considerando una dipendenza più generica: " se i predecessori di un elemento x appartengono a un altro tile, allora il blocco attuale è dipendente dal precedente". Supponendo di costruire il DAG con i tile $G(V,E)$ con $V={(i,w)}$ e $(x,y) in E$ allora dati due blocchi A e B $A arrow B arrow.l arrow "Pred"(B) inter A eq.not emptyset$ con $"Pred"(B)=union_(x in B) "Pred"(x)$. Una volta costruito il grafo si assegnano dei livelli ad ogni tile in base alla dipendenze, ognuno di questi livelli costituisce una frontiera di esecuzione di questo DAG i cui elementi si possono risolvere in parallelo, ad ogni elemento viene poi applicata la ricorrenza originale che risolve il problema dello zaino. L'algoritmo completo viene riassunto di seguito.

#KpDpDag

Per funzionare, la procedura presuppone che i vertici all'interno del grafo siano ordinati in ordine topologico, come si vedrà nell'algoritmo successivo questa condizione viene raggiunta implicitamente inserendo i vertici in ordine row-major. L'algoritmo si articola nelle seguenti fasi: 1) Creazione del DAG. 2) Assegnamento del livello ai vertici, che corrisponde alla loro frontiera di esecuzione. 3) Esecuzione in parallelo di ogni frontiera e ritorno del risultato. La generazione del DAG segue tutte le regole espresse precedentemente e riassunte nella procedura di seguito.


#GenerateDAG

Questa procedura è piuttosto generale e dipende fortemente da come si definisce formalmente il predecessore di una cella, nel caso del knapsack una cella può avere due predecessori $"DP"[i-1][w]$ in caso si decida di non prendere l'elemento è $"DP"[i-1][w-w_i]$ nel caso in cui invece si decida di prenderlo. Per un certo elemento $i$ e un certo range di capacità $(c,c+g)$ dove g rappresenta quante celle sulla linea si vogliono raggruppare nel tile i predecessori sono due range $"DP"[i-1][c-w_i...c-w_i+g]$ e $"DP"[i-1][c...c+g]$ che rappresentano le due casistiche, a questo punto per ordinare l'esecuzione dei tile basta controllare quale dei tile contiene il range e aggiungere un arco tra quel blocco e quello corrente, si noti che i vertici vengono aggiunti in ordine row-major, questo garantisce che l'ordinamento topologico sia rispettato senza dover ricorrere a un operazione di ordinamento topologico esplicita. L'assegnamento dei livelli avviene utilizzando questa definizione in modo analogo
#AssignLevels



Questa piccola funzione implementa sostanzialmente questa ricorrenza $L(v)= cases(0 "if" "Pred"(v) = emptyset, 1+max_(x."level" in "Pred"(v)))$ che prende il massimo livello tra tutti gli elementi dei blocchi predecessori. Questa parte è piuttosto critica, siccome il livello dei predecessori deve essere assegnato prima del blocco corrente i vertici devono essere esplorati in ordine topologico, tuttavia per le condizioni precedenti questa condizione ci viene garantita dalla fase di costruzione stessa del DAG. A questo punto manca solo l'ultima fase di calcolo della funzione nei vari tile

#ComputeTile

Per ogni tile si procede con il consueto calcolo sulla DP e si ricava il risultato.

== Knapsack DP DAG: Risultati
