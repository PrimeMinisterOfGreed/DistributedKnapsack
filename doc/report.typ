#import "@preview/algorithmic:1.0.7"
#import algorithmic: algorithm-figure, style-algorithm
#show: style-algorithm.with(hlines: (
  grid.hline(stroke: 0.5pt + luma(150)),
  grid.hline(stroke: 0.5pt + luma(150)),
  grid.hline(stroke: 0.5pt + luma(150)),
))

#set document(
  title: "Knapsack distribuito: implementazione, comparazione e analisi",
  author: "Matteo Ielacqua",
)
#set text(lang: "it", region: "it", size: 11pt)
#set page(paper: "a4", margin: (x: 1in, y: 1in))
#set par(justify: true)
#set heading(numbering: "1.1.1")
#set figure(numbering: "1.1")
#show figure.where(kind: "listing"): set figure(supplement: [Listato])
#show figure.where(kind: "algorithm"): set figure(supplement: [Algoritmo])

// Helpers for the algorithmic package
#let reqline(body) = Line[#text(style: "italic")[Require: ] #body]
#let ensureline(body) = Line[#text(style: "italic")[Ensure: ] #body]
#let states(..parts) = Line[#parts.join(", ")]

// C++ code listing with line numbers
#let codelisting(caption, srcraw, lang: "cpp") = {
  let lines = srcraw.text.split("\n")
  if lines.last() == "" { lines = lines.slice(0, lines.len() - 1) }
  figure(
    block(
      background: luma(242),
      radius: 4pt,
      inset: 8pt,
      grid(
        columns: (auto, 1fr),
        column-gutter: 8pt,
        ..lines
          .enumerate()
          .map(x => (
            text(size: 7pt, fill: luma(150))[#(x.at(0) + 1)],
            raw(x.at(1), lang: lang, size: 8pt, block: false),
          ))
          .flatten(),
      ),
    ),
    caption: caption,
    kind: "listing",
  )
}

#align(center)[
  #text(size: 17pt, weight: "bold")[Knapsack distribuito: implementazione, comparazione e analisi]
  #v(0.4em)
  #text(size: 12pt)[Matteo Ielacqua]
  #v(1em)
]

= Introduzione

L'algoritmo dello zaino è un noto problema di ottimizzazione. L'idea è che si ha a disposizione uno zaino
e diversi elementi ad ognuno dei quali è assegnato un peso e un profitto. L'obiettivo è di massimizzare il profitto mantenendo il peso
totale degli elementi entro un limite prefissato. Nella versione detta 0/1, ogni elemento può essere preso una sola volta,
il problema si pone quindi come
#align(center)[$ op("max") sum v_i x_i $]
dove $v_i$ è il profitto dell'oggetto i-esimo e $x_i$ rappresenta la scelta (1 se scelto 0 altrimenti), soggetto a
#align(center)[$ sum w_i x_i <= W space and space x_i in {0, 1} $]

Le versioni più complete di questo problema prevedono che si possa scegliere lo stesso elemento più volte, sia con che senza limite superiore a queste scelte
(rispettivamente Bounded o Unbounded). Esistono anche versioni più complesse, come lo zaino multidimensionale, dove non esiste un solo vincolo
(come nel caso più semplice il peso) ma esistono diversi vincoli da tenere in considerazione durante il calcolo. Questa classe di problemi gioca
un ruolo chiave in diversi campi molto delicati ad esempio nelle operazioni logistiche o nello scheduling di task satellitari, in cui infatti
stanno prendendo piede diverse soluzioni basate su euristiche.

== Soluzioni per knapsack 0/1

Il problema è NP-completo, data una soluzione non esiste quindi un algoritmo che possa verificare se è ottima in tempo polinomiale.
Esiste tuttavia una soluzione che lo risolve in tempo pseudo-polinomiale basata sulla programmazione dinamica.
La prima soluzione più semplice e meno ottimale ha complessità temporale $O(2^n)$ ed è basata su un albero decisionale esplorato ricorsivamente

#algorithm-figure(
  "Knapsack 0/1 Ricorsivo",
  {
    import algorithmic: *
    reqline[$"profits"$, $"weights"$, $"max_weight"$, $"index"$]
    ensureline[Profitto massimo ottenibile]
    If([$index = 0$ *or* $max_weight = 0$], {
      Return[$0$]
    })
    Assign[$pick$][$0$]
    If([$weights[index - 1] <= max_weight$], {
      Assign[$pick$][$profits[index - 1] + #Call[Knapsack][$max_weight - weights[index - 1]$, $profits$, $weights$, $index - 1$]$]
    })
    Assign[$not_pick$][$#Call[Knapsack][$max_weight$, $profits$, $weights$, $index - 1$]$]
    Return[op("max")($pick$, $not_pick$)]
  },
) <alg:knapsack-rec>

Le altre due soluzioni sono basate sulla programmazione dinamica.
la prima usa un approccio top-down (memoization) proponendo sostanzialmente una naturale evoluzione dell'approccio precedente,
ha una complessità temporale e spaziale $O(n * W)$

#algorithm-figure(
  "Knapsack 0/1 Top-Down (memoization)",
  {
    import algorithmic: *
    reqline[$profits$, $weights$, $max_weight$, $index$, $table$]
    ensureline[Profitto massimo ottenibile]
    If([$index = 0$ *or* $max_weight = 0$], {
      Return[$0$]
    })
    If([$table[index][max_weight] != -1$], {
      Return[$table[index][max_weight]$]
    })
    Assign[$pick$][$0$]
    If($weights[index - 1] <= max_weight$, {
      Assign[$pick$][$profits[index - 1] + #Call[Knapsack][$max_weight - weights[index - 1]$, $profits$, $weights$, $index - 1$, $table$]$]
    })
    Assign[$not_pick$][$#Call[Knapsack][$max_weight$, $profits$, $weights$, $index - 1$, $table$]$]
    Assign[$table[index][max_weight]$][op("max")($pick$, $not_pick$)]
    Return[$table[index][max_weight]$]
  },
) <alg:knapsack-memo>

la seconda è una versione bottom-up (tabulation) con la medesima complessità ma che evita la ricorsione

#algorithm-figure(
  "Knapsack 0/1 Bottom-Up (tabulation)",
  {
    import algorithmic: *
    reqline[$profits$, $weights$, $max_weight$, $len$]
    ensureline[Profitto massimo ottenibile]
    Assign[$table$][Call[Matrix][$len + 1$, $max_weight + 1$]]
    For([$i \gets 0$ *to* $len$], {
      For([$j \gets 0$ *to* $max_weight$], {
        If(
          [$i = 0$ *or* $j = 0$],
          {
            Assign[$table[i][j]$][$0$]
          },
          {
            Assign[$pick$][$0$]
            If($weights[i - 1] <= j$, {
              Assign[$pick$][$profits[i - 1] + table[i - 1][j - weights[i - 1]]$]
            })
            Assign[$not_pick$][$table[i - 1][j]$]
            Assign[$table[i][j]$][op("max")($pick$, $not_pick$)]
          },
        )
      })
    })
    Return[$table[len][max_weight]$]
  },
) <alg:knapsack-tab>

Una soluzione più raffinata di questa prevede di usare solo l'ultima riga calcolata, in modo da ridurre la complessità spaziale a $O(W)$

#algorithm-figure(
  "Knapsack 0/1 Bottom-Up (spazio ottimizzato)",
  {
    import algorithmic: *
    reqline[$profits$, $weights$, $max_weight$, $len$]
    ensureline[Profitto massimo ottenibile]
    Assign[$table$][Call[array][$0 dots max_weight$] inizializzato a 0]
    For([$i \gets 1$ *to* $len$], {
      For([$j \gets max_weight$ *downto* $weights[i - 1]$], {
        Assign[$table[j]$][op("max")($table[j]$, $table[j - weights[i - 1]] + profits[i - 1]$)]
      })
    })
    Return[$table[max_weight]$]
  },
) <alg:knapsack-opt>

Lo svantaggio principale delle soluzioni basate su programmazione dinamica è che non lavorano bene su casi in cui i pesi non sono interi
(o non possiedono una differenza prefissata tra di loro). Lo stesso vale per l'approccio naive descritto nella prima soluzione.

== Soluzione Greedy

La soluzione Greedy ha un indubbio vantaggio rispetto a tutte le altre, può scegliere anche frazioni di un oggetto,
l'idea è quella di prendere gli oggetti basandosi sul loro rapporto peso/valore, prendendoli in ordine decrescente di rapporto,
finchè lo zaino non è pieno. Se un oggetto non è possibile prenderlo per intero allora si può prendere una sua frazione a seconda
della capacità rimanente.

#algorithm-figure(
  "Fractional Knapsack (Greedy)",
  {
    import algorithmic: *
    reqline[$profits$, $weights$, $max_weight$, $len$]
    ensureline[Profitto massimo ottenibile]
    Assign[$items$][Call[sort][Call[zip][$profits$, $weights$], key = $("profit")/("weight")$ *decrescente*]]
    Assign[$profit$][$0.0$]
    Assign[$current_capacity$][$max_weight$]
    For([$i \gets 0$ *to* $len - 1$], {
      If(
        $items[i].weight <= current_capacity$,
        {
          Assign[$profit$][$profit + items[i].profit$]
          Assign[$current_capacity$][$current_capacity - items[i].weight$]
        },
        {
          Assign[$profit$][$profit + (items[i].profit)/(items[i].weight) times current_capacity$]
          Break()
        },
      )
    })
    Return[$profit$]
  },
) <alg:fknapsack>

= Soluzioni parallele al Knapsack 0/1

Le soluzioni a questo problema sono le più disparate e negli anni ne sono state scritte molte. In ambito aereospaziale il knapsack è usato sopratutto
per lo scheduling di task operativi satellitari @surveymethods in particolare per l'osservazione terrestre @exactmethodphoto.
Un interessante modello di esecuzione è denominato COPA @knapsackCopaSolution e sviluppa un algoritmo parallelo cost optimal per
risolvere il problema sia su CPU con OpenMP che su GPU con CUDA, l'idea dietro quest'algoritmo è la seguente: 1) Si prende
l'array di oggetti $V$ e lo si divide in 2 insiemi disgiunti. 2) Per il primo insieme si generano tutte le $N = 2^(n\/2)$ possibili
combinazioni $a_i$, si calcola di ogni sottoinsieme $a_i$ peso e profitto $w_i, p_i$ e si aggiunge questa tripletta in una lista
in ordine crescente. 3) Si esegue lo stesso con la seconda lista per ogni sottoinsieme $b_i$ e le triplette si mettono in una lista
di ordine non crescente. In questo modo si è generata la lista A, per generare la lista B si riproducono gli step precedenti
semplicemente inserendo le triplette in una lista di ordine non crescente. A questo punto si può passare alla fase di ricerca
che prenderà in input le due liste ordinate.

#algorithm-figure(
  "Algoritmo di ricerca",
  {
    import algorithmic: *
    reqline[Due liste ordinate $A$ e $B$]
    ensureline[Soluzione finale $Bestvalue$ e $X$]
    Line[Inizializza $MaxB_N \gets b_N.p$, $L_N \gets N$, $Bestvalue \gets 0$, $X_1 \gets (0, 0)$]
    For([$i \gets N - 1$ *downto* $1$], {
      If(
        [$b_i.p > MaxB_(i+1)$],
        {
          states[$MaxB_i \gets b_i.p$ *and* $L_i \gets i$]
        },
        {
          states[$MaxB_i \gets MaxB_(i+1)$ *and* $L_i \gets L_(i+1)$]
        },
      )
    })
    states[$i \gets 1$ *and* $j \gets 1$]
    While([$i <= N$ *and* $j <= N$], {
      If([$a_i.w + b_j.w > c$], {
        states[$j \gets j + 1$ *and continue*]
      })
      If($a_i.p + MaxB_j > Bestvalue$, {
        states[$Bestvalue \gets a_i.p + MaxB_j$ *and* $X_1 \gets (a_i, b_(L_j))$]
      })
      Assign[$i$][$i + 1$]
    })
    Line[Converti le due componenti decimali di $X_1$ in due numeri binari: $X_1 \gets (binary_1, binary_2)$]
    LineComment(Assign[$X$][Call[strcat][$binary_1$, $binary_2$]], [Concatena i due binari])
  },
) <alg:cap>

Questo modello di esecuzione si può vedere subito che si presta bene ad essere parallelizzato, sia nella parte di generazione
delle liste sia in quella poi di ricerca dell'ottimo.

== COPA: Cost Optimal Parallel Algorithm


L'algoritmo parallelo è stato pensato per essere risolto su un modello EREW (Exclusive Read, Exclusive Write) PRAM con shared memory e assume $k$ processori accessibili.
L'algoritmo sfrutta prima un'algoritmo di generazione parallela delle liste A e B

#algorithm-figure(
  "Algoritmo parallelo di generazione",
  {
    import algorithmic: *
    reqline[Elementi $(v_i, v_i.w, v_i.p)$, $i = 1, 2, dots, n\/2$]
    ensureline[Lista ordinata $A_(n/2)$ (i.e. $A$)]
    Line[Inizializza $A_0 \gets [(0, 0, 0)]$ e $IC \gets 1$]
    For($i \gets 0$ * to * $n\/2 - 1$, {
      Assign[$v_(i+1)$][$IC$]
      For([#strong[all] $P_j$, $1 <= j <= k$, #strong[in parallel]], {
        Line[Genera nuova lista $A'_i$ aggiungendo la tripletta $(v_(i+1), v_(i+1).w, v_(i+1).p)$ a tutti gli elementi della lista ordinata $A_i$]
        Line[Chiama l'algoritmo di merge parallelo per unire $A_i$ e $A'_i$ in ordine non decrescente di peso, producendo la lista ordinata $A_(i+1)$]
      })
      Assign[$IC$][$IC + 1$]
    })
  },
) <alg:parallel_gen>

l'algoritmo di merging parallelo si articola nel modo seguente: prende in input due vettori ordinati $U = (u_1, u_2, dots, u_m)$ e $V = (v_1, v_2, dots, v_m)$ e $k$ processori $P_1, P_2, dots, P_k$, con $1 <= k <= 2m$ potenza di 2, e produce un vettore ordinato di lunghezza $2m$ ottenuto dall'unione di $U$ e $V$.

+ #text(weight: "bold")[Step 1:] I $k$ processori partizionano $U$ e $V$ in parallelo, ciascuno in $k$ sottovettori (che possono essere vuoti) $(U_1, U_2, dots, U_k)$ e $(V_1, V_2, dots, V_k)$ tali che: $|U_i| + |V_i| = 2m\/k$ per $1 <= i <= k$, e i pesi di tutti gli elementi in $U_i$ e $V_i$ sono minori dei pesi di tutti gli elementi in $U_(i+1)$ e $V_(i+1)$, per $1 <= i <= k - 1$.
+ #text(weight: "bold")[Step 2:] Il processore $P_i$ esegue il merge sequenziale di $U_i$ e $V_i$, per $1 <= i <= k$, in parallelo con gli altri processori.

A questo punto si può passare a dividere equamente tra i processori disponibili i blocchi ottenuti e calcolare per ognuno di questi blocchi gli elementi di profitto massimo.

#algorithm-figure(
  "ALgoritmo di divisione e ricerca del massimo profitto per blocco",
  {
    import algorithmic: *
    reqline[Due liste ordinate $A$ e $B$]
    ensureline[$MaxA$ e $MaxB$]
    Line[Dividi sia $A$ che $B$ uniformemente in $k$ blocchi]
    For([#strong[all] $P_i$, $1 <= i <= k$, #strong[in parallel]], {
      states[$MaxA_i \gets a_(i,1).p$, $MaxB_i \gets b_(i,1).p$]
      For([$j \gets 2$ *to* $e$], {
        If([$a_(i,j).p > MaxA_i$], {
          Assign[$MaxA_i$][$a_(i,j).p$]
        })
        If($b_(i,j).p > MaxB_i$, {
          Assign[$MaxB_i$][$b_(i,j).p$]
        })
      })
    })
  },
) <alg:parallel_max>

Ora lo spazio delle soluzioni è davvero molto grande, per questo motivo si ricorre a un algoritmo di potatura dell'insieme delle soluzioni

#algorithm-figure(
  "Algoritmo di pruning parallelo",
  {
    import algorithmic: *
    reqline[Due liste di blocchi $A = {A_1, A_2, dots, A_k}$ e $B = {B_1, B_2, dots, B_k}$, capacità $c$]
    ensureline[Insieme di coppie di blocchi non potate $remaining$]
    Line[Inizializza $remaining \gets emptyset$]
    For([#strong[all] $P_i$, $1 <= i <= k$, #strong[in parallel]], {
      Line[Inizializza $local_results \gets emptyset$]
      For([$j \gets i$ *to* $k + i - 1$], {
        Assign[$b_idx$][$j mod k$]
        Assign[$Z$][$A_i."front".w + B_(b_idx)."back".w$]
        Assign[$Y$][$A_i."back".w + B_(b_idx)."front".w$]
        If([$Y <= c$], {
          Comment[Tutte le coppie nel blocco sono valide]
          If([$A_i."maxValue" + B_(b_idx)."maxValue" > best_value$], {
            Assign[$best_value$][$A_i."maxValue" + B_(b_idx)."maxValue"$]
          })
          states[$local_results."add"(A_i, B_(b_idx))$]
        })
        ElseIf(
          [$Z <= c$ *and* $Y > c$],
          {
            Comment[Alcune coppie potrebbero essere valide]
            states[$local_results."add"(A_i, B_(b_idx))$]
          },
          {
            Comment[$Z > c$: nessuna coppia valida, pota il blocco]
            Line[*skip*]
          },
        )
      })
    })
    Line[$remaining \gets union_(i = 1)^k local_results_i$]
  },
) <alg:parallel_pruning>

L'algoritmo di pruning sfrutta il fatto che i blocchi sono ordinati per peso. Per ogni coppia di blocchi $(A_i, B_j)$, vengono calcolati due valori chiave:
- $Z = A_i."front".w + B_j."back".w$: il peso minimo possibile combinando il primo elemento di $A_i$ con l'ultimo di $B_j$
- $Y = A_i."back".w + B_j."front".w$: il peso massimo possibile combinando l'ultimo elemento di $A_i$ con il primo di $B_j$

La logica di potatura è la seguente:

+ Se $Y <= c$, tutte le combinazioni tra i due blocchi rispettano il vincolo di capacità. La coppia di blocchi viene mantenuta e il loro massimo profitto combinato viene aggiornato.
+ Se $Z <= c < Y$, alcune combinazioni potrebbero essere valide mentre altre no. La coppia di blocchi viene mantenuta per un'analisi più approfondita.
+ Se $Z > c$, nessuna combinazione tra i due blocchi può rispettare il vincolo di capacità. La coppia viene scartata (potata).

A questo punto è possibile passare alla ricerca parallela della soluzione. L'algoritmo si compone di due sotto-fasi: il secondo salvataggio parallelo dei massimi (Algoritmo 5 del paper COPA) e la ricerca parallela vera e propria (Algoritmo 6).

=== Secondo salvataggio parallelo dei massimi

Dopo il pruning, al più $2k - 1$ coppie di blocchi vengono distribuite equamente tra i $k$ processori: ogni processore $P_i$ gestisce al massimo $s <= 2$ coppie. Per ogni blocco $B_(it)$ assegnato, si calcola un array di #emph[*suffix max*]: $Max_(i:j)[t]$ rappresenta il profitto massimo tra tutti gli elementi da posizione $j$ a $e$ del blocco $B_(it)$, mentre $L^t_j$ rappresenta l'indice del massimo trovato.

#algorithm-figure(
  "Secondo salvataggio parallelo dei massimi (suffix max)",
  {
    import algorithmic: *
    reqline[Coppie di blocchi rimanenti dopo il pruning]
    ensureline[$Max_(i:j)[t]$ e $L^t_j$ per ogni blocco $B_(it)$]
    For([#strong[all] $P_i$, $1 <= i <= k$, #strong[in parallel]], {
      For([$t \gets 0$ *to* $s - 1$], {
        states[$Max_(i:e)[t] \gets b_(it:e).p$, $L^t_e \gets e$]
        For([$j \gets e - 1$ *downto* $1$], {
          If(
            [$b_(it:j).p > Max_(i:(j+1))[t]$],
            {
              states[$Max_(i:j)[t] \gets b_(it:j).p$, $L^t_j \gets j$]
            },
            {
              states[$Max_(i:j)[t] \gets Max_(i:(j+1))[t]$, $L^t_j \gets L^t_(j+1)$]
            },
          )
        })
      })
    })
  },
) <alg:parallel_suffix_max>

Questo pre-calcolo consente, durante la ricerca, di conoscere in $O(1)$ il miglior profitto ottenibile da un dato punto in poi nella lista $B$, accelerando notevolmente la scansione.

=== Ricerca parallela con two-pointer

Ciascun processore esegue una ricerca #emph[*two-pointer*] sulle proprie coppie di blocchi $(A_i, B_(it))$. Per ogni coppia, due puntatori $x$ (su $A_i$) e $y$ (su $B_(it)$) scorrono i blocchi in direzioni opposte:

#algorithm-figure(
  "Ricerca parallela (two-pointer search)",
  {
    import algorithmic: *
    reqline[Coppie di blocchi rimanenti dopo il pruning, suffix max arrays]
    ensureline[Soluzione finale $Bestvalue$ e $X$]
    For([#strong[all] $P_i$, $1 <= i <= k$, #strong[in parallel]], {
      states[$t \gets 0$, $Maxvalue_i \gets 0$, $X_i \gets (0, 0)$]
      While([$0 <= t <= s - 1$], {
        Line[Seleziona la coppia di blocchi $(A_i, B_(it))$]
        states[$x \gets 1$, $y \gets 1$]
        While([$x <= e$ *and* $y <= e$], {
          If([$a_(i:x).w + b_(it:y).w > c$], {
            states[$y \gets y + 1$ *and continue*]
          })
          If([$a_(i:x).p + Max_(i:y)[t] > Maxvalue_i$], {
            Assign[$Maxvalue_i$][$a_(i:x).p + Max_(i:y)[t]$]
            Assign[$X_i$][$(a_(i:x), b_(it:L^t_y))$]
          })
          Assign[$x$][$x + 1$]
        })
        Assign[$t$][$t + 1$]
      })
    })
    Line[*Riduzione globale:* $Bestvalue \gets Maxvalue_1$, $X \gets X_1$]
    For($i \gets 2$ * to * $k$, {
      If($Bestvalue < Maxvalue_i$, {
        states[$Bestvalue \gets Maxvalue_i$, $X \gets X_i$]
      })
    })
    Line[Converti le componenti di $X$ in binario e concatena per ottenere la soluzione finale]
  },
) <alg:parallel_search>

La logica è la seguente: poiché $A_i$ è ordinato per peso crescente e $B_(it)$ per peso decrescente, quando la somma dei pesi supera la capacità $c$, si incrementa $y$ per passare a un elemento più leggero in $B$. Quando invece la somma è ammissibile, il profitto candidato si calcola come $a_(i:x).p + Max_(i:y)[t]$, dove $Max_(i:y)[t]$ rappresenta il miglior profitto ottenibile da $y$ in poi nel blocco $B_(it)$. Questo evita di dover iterare su tutte le coppie di elementi, riducendo la complessità a $O(e) = O(N\/k)$.

La complessità complessiva dello stage di ricerca parallela è $O(4N\/k + k)$, che nel caso $k = O(N^(1\/2))$ diventa $O(2^(n\/4))$.

= Implementazione dell'esperimento

l'esperimento si divide in 4 parti distinte: 1) l'implementazione del algoritmo di risoluzione del knapsack con programmazione dinamica: questo metodo è quello più semplice da implementare ed è stato usato come base per verificare la corretta implementazione negli unit test degli algoritmi successivi. 2) Implementazione dell'algoritmo con programmazione dinamica in MPI 3) Implementazione dell'algoritmo COPA sequenziale e parallelo in shared memory usando OpenMP. 4) Implementazione dell'algoritmo COPA distribuito usando MPI

== Implementazione del knapsack in dynamic programming

L'implementazione del knapsack in programmazione dinamica sequenziale è decisamente semplice sia nella versione sequenziale che parallela in shared memory:

#codelisting(
  "Knapsack dynamic programming sequenziale",
  ```cpp
  int n = weights.size();
  std::vector<std::vector<int>> dp(n + 1, std::vector<int>(capacity + 1, 0));
  // Build the dp table
  for (int i = 1; i <= n; ++i) {
      #pragma omp parallel for num_threads(numThreads) // Adjust the number of threads as needed
      for (int w = 0; w <= capacity; ++w) {
          if (weights[i - 1] <= w) {
              dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
          } else {
              dp[i][w] = dp[i - 1][w];
          }
      }
  }
  ```,
)

Questo algoritmo ha una complessità di $O(N * C)$ dove C è la capacità dello zaino, la parallelizzazione avviene scomponendo il ciclo for interno che scrive i valori nella tabella, non è possibile invece parallelizzare il ciclo for più esterno perchè per risolvere la linea successiva bisogna prima calcolare tutti i pesi di quella precedente.

=== Implementazione MPI

L'implementazione MPI dell'algoritmo è molto simile, l'approccio seguito è stato quello di assegnare la tabella a un master, che scompone la linea successiva da calcolare in task di equa lunghezza da distribuire ai vari worker.

#codelisting(
  "Worker task",
  ```cpp
  using namespace boost::mpi;
  int n = static_cast<int>(weights.size());
  std::vector<uint32_t> line(capacity + 1, 0);
  for (int i = 1; i <= n; ++i)
  {
      broadcast(comm, line.data(), static_cast<int>(line.size()), 0);
      while (true)
      {
          NodeTask task{};
          auto status = comm.recv(any_source, any_tag, task);
          if (status.tag() == NODETAG::TERMINATE)
          {
              break;
          }
          int len = task.endIndex - task.startIndex + 1;
          std::vector<uint32_t> result(len);
          for (int w = task.startIndex; w <= task.endIndex; ++w)
          {
              if (weights[i - 1] <= w)
                  result[w - task.startIndex] = std::max(line[w], line[w - weights[i - 1]] + values[i - 1]);
              else
                  result[w - task.startIndex] = line[w];
          }

          comm.send(status.source(), NODETAG::RESPONSE, NodeResponse{task.startIndex, task.endIndex});
          comm.send(status.source(), NODETAG::DATA, NodeResponseData{std::move(result)});
      }
      comm.barrier();
  }
  ```,
)

i task sono dei range di indici che il worker deve computare, una volta finito il worker invia al master il blocco appena computato dopodichè il worker attende tutti i nodi alla barriera e si comincia il calcolo per il prossimo oggetto.

#codelisting(
  "Master Task",
  ```cpp
  int n = static_cast<int>(weights.size());
  std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));
  for (int i = 1; i <= n; ++i)
  {
      broadcast(MPI_COMM_WORLD,dp[i - 1].data(),capacity+1,0); //broadcast the previous line from master
      int actual_index = 0;
      int active = 0;
      auto generator = [capacity, chunk_size,&actual_index,&active]() -> std::optional<NodeTask> {
          if (actual_index > capacity)
          {
              return std::nullopt;
          }
          int start = actual_index;
          int end = std::min(actual_index + chunk_size - 1, capacity);
          actual_index = end + 1;
          active++;
          return NodeTask{start, end};
      };
      send_a_task_to_all_workers(comm, generator);
      while(active>0)
      {
          auto result= receive_a_result_from_any_worker(comm);
          active--;
          copy_result_to_dp_table(dp[i], result);
      }
      terminate_all_workers(comm);
      comm.barrier();
  }
  ```,
)

terminati i cicli il master può leggere dalla DP il risultato. Si evince subito che l'implementazione MPI è piuttosto semplice e segue uno schema molto simile alla versione Shared Memory, tuttavia presenta lo svantaggio di dover comunicare la linea calcolata a tutti i worker. Svantaggio che si avrebbe comunque anche nel caso della versione più ottimizzata, in cui non esiste la DP perchè non si esegue il backtracking per ricostruire gli oggetti scelti, ma si calcola solo il profitto massimo ottenibile. In questo caso infatti, per calcolare la linea successiva della DP è necessario conoscere la linea precedente, quindi anche in questo caso si dovrebbe comunicare a tutti i worker la linea precedentemente calcolata.

== Knapsack COPA

La versione COPA dell'algoritmo è stata pensata per essere eseguita in shared memory e per ovviare a un problema della versione in dynamic programming dell'algoritmo, cioè che la grandezza della tabella cresce con l'aumentare della capacità dello zaino. Invece nella versione COPA il numero di soluzioni da esplorare è perlopiù legato al numero degli elementi, tuttavia questo insieme cresce con un ritmo di $2^(n\/2)$, quindi per un numero di elementi superiore a 40-50 il numero di combinazioni diventa proibitivo da memorizzare (come anche sottolineato nell'articolo originale).

=== Algoritmo di generazione dei sottoinsiemi

I sottoinsiemi sono generati partendo dalla lista generale, dividendola in due e generando a partire dalle due liste spezzate un insieme di sottoinsiemi di soluzioni. L'algoritmo si articola in due fasi: 1) copia della lista precedente e aggiunta dell'elemento a ogni insieme 2) merging della nuova lista con quella precedente usando l'algoritmo di coranking.

#codelisting(
  "funzione di Parallel Merging",
  ```cpp
  using namespace std::ranges;
  int total_size = static_cast<int>(A.size() + B.size());

  #pragma omp parallel for num_threads(num_threads)
  for (int t = 0; t < num_threads; ++t)
  {
      int start = t * total_size / num_threads;
      int end = (t + 1) * total_size / num_threads;

      auto [a_start, b_start] = co_rank(A, B, start);
      auto [a_end, b_end] = co_rank(A, B, end);
      std::merge(begin(A) + a_start, begin(A) + a_end, begin(B) + b_start, begin(B) + b_end, begin(output) + start);
  }
  ```,
)

#codelisting(
  "Funzione di generazione e merging",
  ```cpp
  for (int item_idx = 0; auto &&item : r)
  {
      std::vector<CopaSubset> shifted{subsets.size()};
      auto [w, v] = item;
      for (int i = 0; i < static_cast<int>(shifted.size()); i++)
      {
          shifted[i] = subsets[i];
          shifted[i].addItem(item_idx, w, v);
      }
      item_idx++;
      std::vector<CopaSubset> merged;
      merged.resize(subsets.size() + shifted.size());
      parallel_merge(subsets, shifted, merged, numthreads);
      subsets = std::move(merged);
  }
  ```,
)

Per aumentare il più possibile la località dei dati durante questa fase, si copia il dato e gli si aggiunge l'item direttamente nelle istruzioni successive. Questa funzione nello specifico restituisce il metodo di generazione della lista A in ordine crescente, per la lista B è sufficiente usare std::reverse, una possibile ottimizzazione sarebbe quella di usare std::ranges::reverse in modo che il reversing dei dati sia lazy, cioè effettuato solo quando si pesca dal range. Tuttavia visto che le funzioni successive richiederanno l'uso di input range, che richiede che il range in entrata sia contiguo, questa operazione non sarà possibile poichè il reverse range non è contiguo per lo standard c++. A questo punto si può procedere e suddividere la lista di blocchi in sottoinsiemi di blocchi equamente distribuiti tra i processori, avendo cura di calcolare il massimo profitto durante questa operazione.

#codelisting(
  "Funzione di calcolo del massimo e distribuzione dei blocchi",
  ```cpp
  int n = static_cast<int>(std::ranges::size(input));
  int k = num_threads;
  int block_size = n / k;
  int remainder = n % k;
  #pragma omp parallel for num_threads(num_threads)
  for (int i = 0; i < k; ++i)
  {
      int start = i * block_size + std::min(i, remainder);
      int end = start + block_size + (i < remainder ? 1 : 0);
      if (start >= end)
      {
          output[i] = {std::span<const CopaSubset>{}, 0};
          continue;
      }
      auto span = std::ranges::subrange(std::ranges::begin(input) + start, std::ranges::begin(input) + end);
      int max_val = 0;
      for (const auto &elem : span)
      {
          if (elem.totalValue > max_val)
              max_val = elem.totalValue;
      }
      output[i] = {span, max_val};
  }
  ```,
)

A questo punto ci si ritrova in output una lista di sottoinsiemi di blocchi, ogni elemento corrisponde al sottoinsieme su cui ci si aspetta che il processo p operi.

=== Pruning delle soluzioni

Una volta compiuta l'operazione di distribuzione si può passare a eseguire il pruning delle soluzioni.

#codelisting(
  "Funzione di pruning di un sottoinsieme di blocchi",
  ```cpp
  int k = static_cast<int>(blocksB.size());
  int best_value = 0;
  for (int j = i; j < k + i; ++j)
  {
      int b_idx = j % k;
      const auto &blockB = blocksB[b_idx];
      if (blockB.block.empty())
          continue;
      int Z = blockA.block.front().totalWeight + blockB.block.back().totalWeight;
      int Y = blockA.block.back().totalWeight + blockB.block.front().totalWeight;

      // Note: prune is done by not adding the pair to local_results
      if (Y <= capacity)
      {
          // All pairs in this block pair are valid; save max profit and prune
          if (blockA.maxValue + blockB.maxValue > best_value)
          {
              best_value = blockA.maxValue + blockB.maxValue;
          }
          remaining.emplace_back(blockA, blockB);
      }
      else if (Z <= capacity && Y > capacity)
      {
          remaining.emplace_back(blockA, blockB);
      }
      else if (Z > capacity)
      {
          // No pairs in this block pair are valid; prune
      }
  }
  ```,
)

La suddetta funzione deve essere chiamata in modo parallelo in questo modo. La suddivisione è necessaria poichè questa implementazione della funzione di pruning è stata riutilizzata anche nella versione MPI.

#codelisting(
  "Chiamata parallela della funzione di pruning",
  ```cpp
  std::vector<std::vector<std::pair<CopaBlock, CopaBlock>>> local_results(threads);
  #pragma omp parallel for num_threads(threads)
  for (int i = 0; i < threads; ++i)
  {
      const auto &blockA = blocksA[i];
      prune_block_pair(blockA, blocksB, local_results[i], capacity, i);
  }
  // Merge local results from all processors
  for (const auto &local : local_results)
  {
      remaining.insert(remaining.end(), local.begin(), local.end());
  }
  ```,
)

A questo punto si può illustrare la funzione di ricerca parallela dei massimi, che viene chiamata sui blocchi rimanenti in questo modo, anche in questo caso la funzione è stata disaccoppiata poichè la parte di calcolo è stata riutilizzata nella versione a memoria distribuita.

#codelisting(
  "Ricerca parallela dei massimi sui blocchi rimanenti",
  ```cpp
  #pragma omp parallel for num_threads(numThreads)
  for (int i = 0; i < numThreads; ++i)
  {
      const auto &[blockA, blockB] = remainingPairs[i];

      // Stage 5: Two-pointer search within this block pair
      BlockPairSearchResult result = block_pair_pointer_search(blockA, blockB, capacity);
      processBestVal[i] = result.bestVal;
      processBestAIdx[i] = result.bestAIdx;
      processBestBIdx[i] = result.bestBIdx;
  }
  ```,
)

La funzione prende in input le coppie rimanenti in ciascuna lista, che è un vettore di coppie di blocchi, e li esamina in questo modo: prima calcola il suffisso massimo di profitto tra i blocchi della lista B, dopodichè esegue una comparazione con i pesi della lista A.

#codelisting(
  "Funzione di ricerca del massimo tra i blocchi rimanenti",
  ```cpp
  int blocka_size = static_cast<int>(blockA.block.size());
  int blockb_size = static_cast<int>(blockB.block.size());
  // Stage 4: Suffix max for this B block
  std::vector<int> suffixMaxVal(blockb_size);
  std::vector<int> suffixMaxIdx(blockb_size);
  block_suffix_max_values(blockB.block, suffixMaxVal, suffixMaxIdx);

  int x = 0, y = 0;
  int bestVal = 0, bestA = 0, bestB = 0;
  while (x < blocka_size && y < blockb_size)
  {
      if (blockA.block[x].totalWeight + blockB.block[y].totalWeight > capacity)
      {
          y++;
          continue;
      }
      int candidate = blockA.block[x].totalValue + suffixMaxVal[y];
      if (candidate > bestVal)
      {
          bestVal = candidate;
          bestA = blockA.block[x].index;
          bestB = suffixMaxIdx[y];
      }
      x++;
  }
  return {bestVal, bestA, bestB};
  ```,
)

La funzione di calcolo dei suffissi è implementata in questo modo

#codelisting(
  "Funzione di calcolo dei suffix max",
  ```cpp
  int e = static_cast<int>(block.size());
  suffixMaxValues[e - 1] = block[e - 1].totalValue;
  suffixMaxIndices[e - 1] = block[e - 1].index;
  for (int j = e - 2; j >= 0; --j)
  {
      if (block[j].totalValue > suffixMaxValues[j + 1])
      {
          suffixMaxValues[j] = block[j].totalValue;
          suffixMaxIndices[j] = block[j].index;
      }
      else
      {
          suffixMaxValues[j] = suffixMaxValues[j + 1];
          suffixMaxIndices[j] = suffixMaxIndices[j + 1];
      }
  }
  ```,
)

=== L'algoritmo completo

La funzione completa si articola quindi in questo modo

#codelisting(
  "Funzione knapsackcopa completa",
  ```cpp
  using namespace std::views;
  auto list = zip(weights, values);
  int n = static_cast<int>(weights.size());
  auto Alist = take(list, n / 2);
  auto Blist = drop(list, n / 2);
  std::vector<CopaSubset> A, B;
  generate_copa_subsets(Alist, numThreads);
  generate_copa_subsets(Blist, numThreads, true);
  int N = static_cast<int>(B.size());
  // Stage 2 : Parallel suffix max for B (MaxBj and Lj)
  std::vector<CopaBlock> blocksA(numThreads);
  std::vector<CopaBlock> blocksB(numThreads);
  distribute_block_per_processor(A, blocksA, numThreads);
  distribute_block_per_processor(B, blocksB, numThreads);
  // Stage 3: Parallel pruning
  std::vector<std::pair<CopaBlock, CopaBlock>> remainingPairs;
  prune(blocksA, blocksB, remainingPairs, capacity, numThreads);
  // Stages 4+5: For each remaining block pair, compute suffix max and search
  int k = static_cast<int>(remainingPairs.size());
  int bestAIdx = 0, bestBIdx = 0, bestValue = 0;
  std::vector<int> localBestVal(k, 0);
  std::vector<int> localBestAIdx(k, 0);
  std::vector<int> localBestBIdx(k, 0);
  parallel_save_max(remainingPairs, localBestVal, localBestAIdx, localBestBIdx, capacity, k);
  // Reduce across all remaining pairs
  #pragma omp parallel for num_threads(numThreads)
  for (int i = 0; i < k; ++i)
  {
      #pragma omp critical
      if (localBestVal[i] > bestValue)
      {
          bestValue = localBestVal[i];
          bestAIdx = localBestAIdx[i];
          bestBIdx = localBestBIdx[i];
      }
  }
  // Reconstruct solution
  KnapsackSolution solution{};
  solution.totalValue = bestValue;
  solution.totalWeight = A[bestAIdx].totalWeight + B[bestBIdx].totalWeight;
  auto items = (A[bestAIdx] << B[bestBIdx]).getItemIndices();
  solution.items.reserve(items.size());
  solution.items.insert(solution.items.end(), items.begin(), items.end());
  return solution;
  ```,
)

Questa funzione mette semplicemente insieme tutti i passi dell'algoritmo: 1) si generano i 2 subsets dividendo in 2 la lista di pesi e profitti 2) si distribuiscono i blocchi tra i processori e si cercano i primi massimi profitti tra i blocchi 3) si esegue la potatura dei blocchi 4) si effettua il secondo algoritmo di ricerca dei massimi e si estrae la soluzione finale concatenando gli indici degli oggetti rappresentati come stringa binaria.

== Knapsack COPA MPI

Questo algoritmo è stato concepito principalmente per un utilizzo in shared memory, non è stato pensato per essere utilizzato invece in memoria distribuita. Tuttavia è possibile farne utilizzo, introducendo delle accortezze per minimizzare il più possibile lo scambio di dati. Partendo dalle funzioni di generazione dei subsets

=== Generazione dei subsets

#codelisting(
  "Funzione di merge parallelo della lista",
  ```cpp
  auto proc = compute_corank_procedure(comm, A, B);
  int rank = comm.rank();
  int a_start = proc.a_displacements[rank];
  int a_end = a_start + proc.a_sizes[rank];
  int b_start = proc.b_displacements[rank];
  int b_end = b_start + proc.b_sizes[rank];
  std::vector<CopaSubset> local_result(proc.output_sizes[rank]);
  std::merge(A.begin() + a_start, A.begin() + a_end, B.begin() + b_start, B.begin() + b_end, local_result.begin());
  all_gatherv(comm, local_result.data(), proc.output_sizes[rank], output.data(), proc.output_sizes,
          proc.output_displacements, 0);
  ```,
)

La funzione di merging funziona calcolando parallelamente tutti i corank degli elementi, infine applica il merge su pezzi di lista distinti, esegue il gather delle varie liste usando come displacement proprio gli indici calcolati dal corank e infine distribuisce il risultato a tutti i task.

#codelisting(
  "Funzione di generazione dei subsets",
  ```cpp
  std::vector<CopaSubset> subsets{CopaSubset{}};
  for (int item_idx = 0; const auto &item : items)
  {
      decltype(subsets) shifted{subsets.size()};
      auto [w, v] = item;
      int local_size = static_cast<int>(shifted.size()) / comm.size();
      int remainder = static_cast<int>(shifted.size()) % comm.size();
      auto procedure = compute_scatter_procedure(comm, subsets);
      std::vector<CopaSubset> local_shifted{static_cast<size_t>(procedure.sizes[comm.rank()]), CopaSubset{}};

      // Distribute subsets to all processes for local shifting
      scatterv(comm, subsets.data(), procedure.sizes, procedure.displacements, local_shifted.data(),
                  procedure.sizes[comm.rank()], 0);

      for (int i = 0; i < static_cast<int>(local_shifted.size()); i++)
      {
          local_shifted[i].addItem(item_idx, w, v);
      }
      all_gatherv(comm, local_shifted.data(), procedure.sizes[comm.rank()], shifted.data(), procedure.sizes,
              procedure.displacements, 0);
      item_idx++;
      std::vector<CopaSubset> newSubsets;
      newSubsets.resize(subsets.size() + shifted.size());
      mpi_parallel_merge(comm, subsets, shifted, newSubsets);
      subsets = std::move(newSubsets);
  }
  if (reverse)
      std::reverse(subsets);
  return subsets;
  ```,
)

La funzione di generazione funziona in modo analogo alla sua controparte shared memory, si crea una lista shifted a partire da quella presente al passo precedente, si esegue lo scattering e ogni task aggiunge internamente al suo pezzo di lista l'oggetto, infine si esegue riunisce la lista e si procede alla funzione di merging parallelo.

=== Distribuzione dei blocchi ai processori

Anche in questo caso la funzione di distribuzione si comporta in modo analogo alla versione shared memory, ogni processore prende un subrange della lista di blocchi e lo salva cercando anche l'elemento di massimo profitto. La funzione non ritorna la lista ma la usa solo per cercare il massimo.

#codelisting(
  "Funzione di distribuzione dei blocchi",
  ```cpp
  using value_type = std::ranges::range_value_t<decltype(input)>;
  // calculate the size of the blocks for the thread
  int n = static_cast<int>(std::ranges::size(input));
  int k = comm.size();
  int i = comm.rank();

  int block_size = n / k;
  int remainder = n % k;

  int start = i * block_size + std::min(i, remainder);
  int end = start + block_size + (i < remainder ? 1 : 0);
  auto span = std::ranges::subrange(std::ranges::begin(input) + start, std::ranges::begin(input) + end);
  int max_val = 0;
  for (const auto &elem : span)
  {
      if (elem.totalValue > max_val)
          max_val = elem.totalValue;
  }
  return {start, end, max_val};
  ```,
)

Dopodichè i processi eseguono il pruning esattamente come nella versione shared memory.

=== Funzione completa

L'aspetto della funzione di calcolo completa è analogo alla versione shared memory, la lista di pesi e profitti viene divisa a metà in 2 liste A e B. Vengono generati i subset per ognuna delle due liste e viene invertito l'ordine per la seconda. Si distribuiscono i blocchi tra i processori e poi ogni processo effettua il pruning comparando i blocchi ad esso assegnati. Finito questo insieme di operazione ogni processo avrà trovato un suo massimo, a quel punto si effettua una reduce su tutte le soluzioni trovate e si prende quella di massimo profitto.

#codelisting(
  "Funzione completa COPA MPI",
  ```cpp
  int world = comm.size();
  int n = static_cast<int>(weights.size());
  std::vector<std::pair<int, int>> Alist, Blist;
  Alist.reserve(n / 2);
  Blist.reserve(n / 2);
  for (int i = 0; i < n / 2; ++i)
      Alist.emplace_back(weights[i], values[i]);
  for (int i = n / 2; i < n; ++i)
      Blist.emplace_back(weights[i], values[i]);
  auto A = mpi_generate_copa_subsets(comm, Alist);
  auto B = mpi_generate_copa_subsets(comm, Blist, true);
  int N = static_cast<int>(B.size());
  auto i = comm.rank();
  // Stage 2 : Parallel suffix max for B (MaxBj and Lj)
  CopaBlock blockA;
  std::vector<CopaBlock> blocksB(world);
  std::vector<CopaDistributionIndex> blockBdescriptors(world);
  auto blockAdesc = mpi_distribute_block_per_process(comm, A);
  auto blockBdesc = mpi_distribute_block_per_process(comm, B);
  boost::mpi::all_gather(comm, blockBdesc, blockBdescriptors.data());
  blockA.block = std::span(A).subspan(blockAdesc.start, blockAdesc.end - blockAdesc.start);
  blockA.maxValue = blockAdesc.maxValue;
  for (int i = 0; i < world; ++i)
  {
      blocksB[i].block =
          std::span(B).subspan(blockBdescriptors[i].start, blockBdescriptors[i].end - blockBdescriptors[i].start);
      blocksB[i].maxValue = blockBdescriptors[i].maxValue;
  }
  // Note: we avoid communication during distribution, each node will only communicate the tasks to perform
  std::vector<std::pair<CopaBlock, CopaBlock>> local_remaining_pairs{};
  comm.barrier();
  prune_block_pair(blockA, blocksB, local_remaining_pairs, capacity, i);
  BlockPairSearchResult solution{};
  for (const auto &[remBlockA, remBlockB] : local_remaining_pairs)
  {
      auto local_solution = block_pair_pointer_search(remBlockA, remBlockB, capacity);
      if (local_solution.bestVal > solution.bestVal)
      {
          solution = local_solution;
      }
  }
  BlockPairSearchResult best{};
  reduce(comm, solution,best,boost::mpi::maximum<BlockPairSearchResult>(),0);
  if (comm.rank() == 0)
  {
      // Reconstruct solution
      KnapsackSolution solution{};
      solution.totalValue = best.bestVal;
      solution.totalWeight = A[best.bestAIdx].totalWeight + B[best.bestBIdx].totalWeight;
      //auto items = (A[best.bestAIdx] << B[best.bestBIdx]).getItemIndices();
      solution.items.reserve(items.size());
      solution.items.insert(solution.items.end(), items.begin(), items.end());
      return solution;
  }
  return {};
  ```,
)

=== Considerazioni finali sull'implementazione

L'implementazione è molto sperimentale e quindi non è sicuramente ottima, ci sono diversi approcci che si possono valutare per esprimere al meglio le capacità dell'algoritmo. Il primo è sicuramente quello di usare una forma più ibrida di calcolo distribuito, in cui ogni task è una farm di thread che collaborano tra di loro in shared memory. Per rendere gli algoritmi più facilmente utilizzabili è stato creato un wrapper python mediante Boost.python, che offre la possibilità di creare classi e funzioni leggibili e eseguibili direttamente dall'interprete, la misura del tempo viene effettuata mediante il modulo time di python, che espone sostanzialmente uno steady clock, azionando l'orologia prima di invocare la funzione C++ e subito dopo alla sua conclusione, per evitare problemi con l'instanza del comunicatore MPI si delega il requisito all'eseguibile python che lo instanzia quando si include la libreria Mpi4py.

=== Strumenti e opzioni di compilazione

La libreria è stata compilata con il compilatore GCC 15.2.0 usando il set di impostazioni di default Release presente in CMake 4.2.0 (quindi -O2 e -DNDEBUG a cui si aggiungono per default -ftree-vectorize e -ffast-math), lo standard usato è C++23 con libstdc++11, l'interprete python è python 3.12.13

= Risultati

Prima dell'analisi dei risultati, bisogna tenere a mente alcune premesse. La prima, la più importante, è che l'algoritmo COPA proposto non può funzionare con più di 50 oggetti, questo vincolo comunque non è particolarmente oneroso per l'analisi, visto che comunque nella versione DP dell'algoritmo si può parallelizzare solo sul ciclo più interno, la cui lunghezza dipende dalla capacità. La seconda è che nel caso di un algoritmo MPI, un implementazione puramente distribuita è sicuramente molto più inefficiente di una ibrida, che però richiederebbe degli accorgimenti ulteriori per essere implementata.

== Shared Memory

i due algoritmi in shared memory hanno efficacia profondamente diversa.

=== Knapsack DP

#figure(
  image("images/t_c_p_shared.png", width: 100%),
  caption: [Confronto dei tempi di esecuzione per gli algoritmi shared memory al variare del numero di thread.],
) <fig:tcpshared>

Dal grafico soprastante si osserva innanzitutto che, superata una certa capacità, i tempi di esecuzione dell'algoritmo DP crescono rapidamente, mentre quelli del COPA rimangono sostanzialmente costanti: questo è dovuto al fatto che la complessità del DP dipende dal prodotto $N times C$ (con $C$ la capacità dello zaino), mentre il COPA dipende solo dal numero di elementi $N$, generando $2^(N\/2)$ sottoinsiemi indipendentemente dalla capacità.

#figure(
  image("images/KnapsackDP_Speedup.png", width: 100%),
  caption: [Speedup dell'algoritmo KPDP in shared memory],
) <fig:kpdpshared>

Lo speedup nel caso della versione in dynamic programming come ci si potrebbe aspettare cresce un pò con l'aumentare massiccio dei threads, questo diviene particolarmente vero quando la capacità è molto alta, sopratutto perchè a quel punto il ciclo interno sarà quello con più lavoro da eseguire. Questo comportamento smette di verificarsi quando non ci sono abbastanza elementi da poter giustificare l'instanziazione di tutti quei thread e quindi si perde di efficacia, ciò è verificabile meglio nel plot sottostante

#figure(
  image("images/KnapsackDP_speedupplot.png", width: 100%),
  caption: [Speedup plot dell'algoritmo KP in DP],
) <fig:kpdpsharedplot>

In questo grafico sono riportati diversi speedup a seconda della capacità, si vede subito comunque che l'algoritmo scala bene solo con i primi threads e solo quando la capacità supera una certa soglia. Diventa ancora più evidente se si osserva il grafico dell'efficienza

#figure(
  image("images/KnapsackDP_Efficiency.png", width: 100%),
  caption: [Efficienza dell'algoritmo KP in DP],
) <fig:kpdpsharedefficiency>

=== Knapsack COPA

Nell'algoritmo COPA si può osservare invece che lo speedup cresce molto rapidamente all'aumentare dei processori ma decresce subito superata una certa soglia

#figure(
  image("images/KnapsackCOPA_Speedup.png", width: 100%),
  caption: [Speedup dell'algoritmo KP COPA in shared memory],
) <fig:kpcopaspeedup>

#figure(
  image("images/KnapsackCOPA_speedupplot.png", width: 100%),
  caption: [Speedup dell'algoritmo KP COPA in shared memory],
) <fig:kpcopaplot>

Non bisogna sorprendersi della discesa repentina dello speedup all'aumentare dei processori, l'algoritmo presenta infatti una peculiarità: ovvero che potrebbe scalare meglio con un numero di elementi più alto, sfortunatamente il numero di soluzioni generate durante lo step di creazione dei subsets lo rende proibitivo a livello di memoria richiesta, visto che lo spazio occupato è $2^n$ elementi. Le linee a capacità bassa che segnano invece uno speedup di 1.4 sono perlopiù falsi positivi dovuti alla chiusura praticamente immediata del job vista l'assenza di coppie valutabili durante la fase di pruning. Ovviamente questo comportamento si riflette sull'efficienza che decresce repentinamente e sui costi che salgono rapidamente sopratutto oltre i 20 processori.

#figure(
  image("images/KnapsackCOPA_Cost.png", width: 100%),
  caption: [Costo dell'algoritmo KP COPA in shared memory],
) <fig:kpcopacost>

#figure(
  image("images/KnapsackCOPA_Efficiency.png", width: 100%),
  caption: [Efficienza dell'algoritmo KP COPA in shared memory],
) <fig:kpcopaefficiency>

=== Knapsack DP vs COPA

Mettendo a confronto i due algoritmi si può osservare che, utilizzando l'algoritmo COPA nelle giuste configurazioni si possono ottenere benefici considerevoli in termini di speedup a un costo contenuto in termini di processori impiegati.

#figure(
  image("images/knapsackdpvscopashared.png", width: 100%),
  caption: [Speedup dell'algoritmo KP COPA vs DP],
) <fig:kpcopavsdp>

Come si può osservare dal grafico, l'algoritmo in dynamic programming vince sostanzialmente in tutte quelle configurazioni in cui la capacità è più piccola di una certa soglia, sotto il $10^7$, mentre invece per configurazioni in cui la capacità è molto grande COPA è molto più veloce. Questo conferma quanto scritto nell'articolo in cui si presenta proprio questa soluzione al problema dello zaino, va detto comunque che la limitazione d'uso a cui si è sottoposti è importante, visto che non è possibile calcolare soluzioni per insiemi più grandi di 50 elementi.

== Distributed Memory

Entrambi gli algoritmi, come si può osservare nel grafico, funzionano molto male quando si utilizzano in memoria distribuita.

#figure(
  image("images/knapsackdp_mpi_mpi.png", width: 100%),
  caption: [Speedup degli algoritmi in distributed memory],
) <fig:kpcopadpmpi>

Nel caso dell'algoritmo in dynamic programming, la motivazione è semplice da individuare: il broadcast della riga ad ogni iterazione comporta un costo alto in termini di tempi di comunicazione. Supponendo di usare un intero a 32 bit, quindi 4 byte, per rappresentare il vettore di pesi e il vettore dei profitti, se si suppone una lunghezza di $10^8$ elementi bisogna trasferire circa 381 Mb ogni volta solo per il broadcast e altrettanti per eseguire il gathering e lo scattering del prossimo vettore da calcolare, rendendo questo algoritmo altamente memory bound. La versione COPA invece non presenta questo problema durante il calcolo, purtroppo durante la fase di generazione dei subsets rende il processo memory bound almeno inizialmente, visto che per ogni elemento è necessario scambiarsi tutta la lista di subsets generati, supponendo di usare 3 interi a 32 bit per rappresentare profitto, peso e indice del blocco servono $2^(n\/2) * 4 * 3 * 2$ al passo finale per entrambe le liste, per cui in totale bisogna trasferire $sum_(i = 1)^n 2^(i\/2) * 4 * 3 * 2$, misura che diventa considerevolmente grande una volta che il numero di elementi diventa abbastanza grande, rendendo di fatto l'algoritmo nuovamente memory bound.

=== Possibili miglioramenti

L'approccio a memoria condivisa utilizzato è molto vicino a quello ottimale, usando un dispositivo come una GPU sarebbero possibili accorgimenti ulteriori per migliorare l'efficienza: l'approccio COPA è infatti stato pensato proprio per un approccio GPU oriented, per esempio nella fase di generazione dei subset sarebbe possibile, avendo abbastanza core a disposizione, aggiungere in pochi step l'elemento corrente a tutti gli elementi della lista shifted. Con opportuni accorgimenti potrebbe essere possibile utilizzare
direttamente la versione OpenMP eseguendo offloading a un dispositivo GPU, oppure si potrebbe usare OpenACC per ottenere il medesimo risultato.

Nell'approccio a memoria distribuita le modifiche possibili sono invece più sostanziose e porterebbero sicuramente a incrementi non banali in termini di prestazioni, prima di tutto l'implementazione di un approccio puramente a memoria distribuita, come si è osservato, porta inevitabilmente al dilatamento dei tempi a causa degli elevati tempi di trasmissione, inoltre nessuno di questi algoritmi è stato pensato per funzionare con un approccio a memoria distribuita ma entrambi per la memoria condivisa. Tuttavia sono comunque possibili dei miglioramenti, in entrambe le versioni invece di adottare una strategia puramente a memoria distribuita si può tentare un approccio ibrido, in cui ai task vengono date più cpu per effettuare operazioni in memoria condivisa, consentendo così di distribuire in modo più grossolano i task e evitando ritardi dovuti alle distribuzioni granulari che avvengono adesso, come i broadcast o gli all_gather utilizzati per condividere quei dati che costituiscono una dipendenza per i passi successivi dell'algoritmo in questione.

== Conclusioni

Lo scopo primario del progetto era comprendere a pieno la difficoltà di implementazione dell'algoritmo e il beneficio ottenibile. L'approccio shared memory ha mostrato di essere promettente, sopratutto per quanto riguarda l'approccio COPA. Tuttavia proprio quell'approccio è assolutamente inutilizzabile nel mondo embedded, dove la memoria è una risorsa molto scarsa e quindi determinati approcci non sono possibili. Comunque se si utilizzasse invece per implementare un ottimizzazione da terra da poi usare in orbita, potrebbe rivelarsi una scelta molto promettende per tutti quei problemi che hanno un vincolo di capacità molto elevato con valori di pesi sugli oggetti estremamente variabili tra di loro.

#bibliography("biblio.bib", title: none)
