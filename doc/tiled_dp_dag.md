# DP Tiled su Grafo (Tiled Knapsack DP over a Dependency DAG)

Questo documento spiega l'algoritmo implementato in
`src/Knapsack/knapsackdpdag.cpp` (e il suo riferimento Python
`test/python/test_new_dp.py`), che risolve il problema dello zaino
0/1 con un **dynamic programming tiled** eseguito come **grafo di
dipendenze (DAG)** tra tile.

- **Scopo**: partizionare la classica tabella DP `(n+1) × (capacity+1)` in
  blocchi (tile), così il lavoro può essere distribuito e, in futuro,
  eseguito in parallelo per "wavefront".
- **Output**: insieme di item selezionati, valore totale e peso totale,
  equivalenti a quelli del DP classico `knapsackdp`.

---

## 1. Il problema: knapsack 0/1

Abbiamo `n` item, ciascuno con peso `w_i` e valore `v_i`, e una capacità
`C`. Dobbiamo scegliere un sottoinsieme di item con peso totale `<= C`
che massimizzi il valore totale.

La soluzione DP classica definisce:

```
dp[i][w] = valore massimo usando i primi i item, con capacità w
```

con la ricorrenza (fonte: [Wikipedia – Knapsack problem](https://en.wikipedia.org/wiki/Knapsack_problem)):

```
dp[i][w] = max( dp[i-1][w],                     // salta l'item i
                dp[i-1][w - w_i] + v_i          // prendi l'item i (se w_i <= w)
              )
dp[0][w] = 0
```

Il risultato è `dp[n][C]`. La tabella è `(n+1) × (C+1)`, con complessità
tempo `O(n·C)` e memoria `O(n·C)`.

**Esempio** (pesi `[1,2,3,4]`, valori `[1,6,10,16]`, `C=7`):

| i \ w | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|------|---|---|---|---|---|---|---|---|
| 0    | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 1    | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 2    | 0 | 1 | 6 | 7 | 7 | 7 | 7 | 7 |
| 3    | 0 | 1 | 6 | 10| 11| 16| 17| 17|
| 4    | 0 | 1 | 6 | 10| 16| 17| 22| 26|

Il valore ottimo è `dp[4][7] = 26`.

---

## 2. L'idea: tiling della tabella

La tabella è enorme. La dividiamo in **tile** rettangolari:

- **`item_block`** = numero di righe per tile (quanti item per blocco) → ogni
  tile copre un intervallo di item `[b·item_block, min((b+1)·item_block, n))`.
- **`cap_block`** = numero di colonne per tile (quante capacità per blocco) →
  ogni tile copre un intervallo di capacità `[q·cap_block, q·cap_block+width)`.

La griglia risultante ha:

```
nb = ceil(n / item_block)          // righe di tile (item-blocks)
nq = ceil((C+1) / cap_block)       // colonne di tile (capacity-blocks)
```

Ogni tile è indicizzato da `(b, q)` con `b ∈ [0, nb)` e `q ∈ [0, nq)`.
L'**ultima riga** e l'**ultima colonna** possono essere troncate
(`width = min(cap_block, C+1 - q·cap_block)`).

**Nota**: questo è il **loop tiling / blocking**, una tecnica classica per
migliorare località e parallelizzare il calcolo
(fonte: [Wikipedia – Loop nest optimization](https://en.wikipedia.org/wiki/Loop_nest_optimization)).

### Diagramma della griglia di tile

```
   capacity →
  q:   0          1          2        (nq colonne, cap_block colonne ciascuna)
b:
0  ┌──────────┐ ┌──────────┐ ┌──────────┐
   │ (0,0)    │ │ (0,1)    │ │ (0,2)    │   ← item-block 0: item [0, item_block)
   │ rows+1 × │ │ rows+1 × │ │ rows+1 × │
   └──────────┘ └──────────┘ └──────────┘
1  ┌──────────┐ ┌──────────┐ ┌──────────┐
   │ (1,0)    │ │ (1,1)    │ │ (1,2)    │   ← item-block 1
   └──────────┘ └──────────┘ └──────────┘
2  ┌──────────┐ ┌──────────┐ ┌──────────┐
   │ (2,0)    │ │ (2,1)    │ │ (2,2)    │   ← ultimo item-block (troncato)
   └──────────┘ └──────────┘ └──────────┘
```

Ogni tile `(b,q)` contiene un **buffer DP appiattito** di
`rows × width` celle (in `NodeData.block`), dove:

- `rows = rows_local + 1` (item del blocco + una riga "boundary")
- `width = min(cap_block, C+1 - q·cap_block)`

La riga `0` del tile è la **boundary**: eredita l'ultima riga del tile sopra
(oppure è tutta zero per `b == 0`). Le righe `1..rows_local` sono calcolate
con la ricorrenza DP.

---

## 3. Le dipendenze tra tile (il DAG)

La chiave dell'algoritmo è capire **da quali tile un tile legge i dati**,
cioè le sue **dipendenze**. Il grafo `Graph` (un `boost::adjacency_list`) ha:

- **un vertice per tile** `(b, q)`, con vertex descriptor `b·nq + q`
  (ordine row-major);
- **un arco `u → v`** = "il tile `v` ha bisogno dei dati di `u`".

Ci sono **tre tipi di dipendenze**:

### 3.1. Tile direttamente sopra (boundary)

La riga `0` di `(b,q)` è l'ultima riga di `(b-1, q)`. Quindi `(b,q)`
dipende sempre da `(b-1, q)` (se `b >= 1`).

```
     (b-1, q)  ────────────────►  (b, q)      riga boundary
```

### 3.2. Tile sopra con sfasamento di peso ("take item")

Quando si prende l'item, si legge a capacità `wp - wi`. L'intervallo di
capacità che ricade nel tile attuale, **shiftato indietro di `wi`**, è:

```
[lo, hi] = [q·cap_block - wi,  (q+1)·cap_block - 1 - wi]
```

clampato a `[0, C]`. Ogni tile della riga sopra `(b-1, qp)` la cui colonna
`qp` interseca `[lo, hi]` è un predecessore.

### 3.3. Tile a sinistra, stessa riga

Se `wp - wi` cade in un blocco di capacità a sinistra (colonna `qp < q`),
il valore si legge dal tile `(b, qp)` già calcolato nella **stessa riga**.

### Diagramma delle dipendenze di un tile

```
         riga sopra (b-1)
         ┌──────┐ ┌──────┐ ┌──────┐
         │(b-1,0)│ │(b-1,1)│ │(b-1,2)│ ...
         └──┬───┘ └──┬───┘ └──┬───┘
            │        │        │   (colonne che intersecano [lo,hi])
            │        │        │
 riga b     ▼        ▼        ▼
         ┌──────┐ ┌──────┐ ┌──────┐
         │(b,0) │►│(b,1) │►│(b,2) │   ← tile a sinistra (qp<q)
         └──────┘ └──▲───┘ └──▲───┘
                     │        │
                     └────────┘   il tile (b,q) legge dal sopra e da sinistra
```

**Il risultato è un DAG**: ogni arco va da un tile che precede in ordine
row-major, quindi i predecessori sono sempre calcolati prima.

---

## 4. Ordine di esecuzione e wavefront

Poiché il grafo è un DAG e l'ordine row-major `(b,q)` è un **ordinamento
topologico valido**, i tile si possono calcolare in quel semplice ordine
(versione sequenziale attuale).

Per il **parallelismo futuro**, `compute_levels()` assegna a ogni vertice un
`level = max(level(predecessori)) + 1`. I vertici con lo **stesso level**
sono indipendenti e formano un **wavefront** (anti-diagonale `b+q = costante`),
calcolabile in parallelo con OpenMP.

```
 level:   0         1         2         3         4
        ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐
        │(0,0) │  │(0,1) │  │(0,2) │  │(0,3) │  ...
        │      │  │(1,0) │  │(1,1) │  │(1,2) │
        │      │  │      │  │(2,0) │  │(2,1) │
        └──────┘  └──────┘  └──────┘  └──────┘
```

Ogni colonna del diagramma sopra è un wavefront: i tile al suo interno non
hanno dipendenze reciproche e possono essere calcolati in parallelo. Questo è
il classico pattern di **DP parallelo per anti-diagonali** (usato, ad esempio,
nell'allineamento di sequenze con l'algoritmo
[Needleman–Wunsch](https://en.wikipedia.org/wiki/Needleman%E2%80%93Wunsch_algorithm)).

> Nota: `solve_dag` non legge gli archi del grafo per calcolare i dati (li
> legge direttamente da `g[v].block`). Gli archi servono a `compute_levels`
> e alla schedulazione parallela: garantiscono che nessun tile venga
> processato prima che i suoi predecessori siano pronti.

---

## 5. Come viene calcolato ogni tile

Per ogni tile `(b,q)` in ordine topologico:

1. **Riga boundary (riga 0)**:
   - se `b == 0`: tutte le celle a zero;
   - altrimenti: per ogni colonna locale `c`, mappa la capacità assoluta
     `wp = q·cap_block + c` nel tile sopra
     (`q_prev = wp / cap_block`, `c_prev = wp - q_prev·cap_block`) e copia
     `block[riga rows-1][c_prev]` del tile sopra.

2. **Righe interne (item del blocco)**: per ogni riga `a` e colonna `c`:
   - `v = block[a][c]` (salta l'item);
   - se `wi <= wp`, calcola `src = wp - wi`:
     - se `src >= lo`: `cand = block[a][src-lo] + vi` (stesso tile);
     - altrimenti: `cand = block[a][c_prev] + vi` del tile sinistro `(b, q_prev)`;
   - `v = max(v, cand)`;
   - scrivi `block[a+1][c] = v`.

Alla fine, il valore ottimo è in `block[riga rows-1][colonna C-(nq-1)·cap_block]`
del tile in basso a destra `(nb-1, nq-1)`.

### Backtracking

Per recuperare **quali item** scegliere, `reconstruct_items()` ripercorre la
tabella da `dp[n][C]`, confrontando `dp[i][w]` con `dp[i-1][w]` come nel DP
classico (leggendo i valori direttamente dai tile tramite la mappatura
`(i//item_block, w//cap_block)`).

---

## 6. Complessità

Il calcolo totale è lo stesso del DP classico: ogni cella della tabella viene
visitata una volta, quindi tempo **O(n·C)** e memoria **O(n·C)**. Il tiling non
cambia la complessità asintotica: cambia solo **come** il lavoro viene
organizzato (distribuito per tile, località di cache, parallelizzabile per
wavefront).

La costruzione del grafo ha costo legato al numero di dipendenze
(`build_graph`), che nel caso peggiore è `O(nb · nq² · item_block)`
(ciclo su ogni tile, ogni item del blocco, ogni colonna).

---

## 7. Fonti e riferimenti

- **Knapsack problem** (DP classico, ricorrenza):  
  https://en.wikipedia.org/wiki/Knapsack_problem
- **Loop tiling / blocking** (partizionare lo spazio di iterazione):  
  https://en.wikipedia.org/wiki/Loop_nest_optimization
- **DP parallelo per anti-diagonali / wavefront** (esempio con allineamento
  di sequenze):  
  https://en.wikipedia.org/wiki/Needleman%E2%80%93Wunsch_algorithm
- **Codice di riferimento Python** (sorgente primaria del DAG specifico):  
  `test/python/test_new_dp.py`
- **Codice C++**:  
  `src/Knapsack/knapsackdpdag.cpp`, `src/Knapsack/knapsackdpdag_impl.hpp`
