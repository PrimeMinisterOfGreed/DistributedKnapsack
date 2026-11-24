
#import "@preview/algorithmic:1.0.7"
#import algorithmic: Line, algorithm-figure, iflike, style-algorithm
#import "@preview/codelst:2.0.2": sourcecode



#let ParallelFor = iflike.with(kw1: "for", kw2: "do in parallel", kw3: "end")

#let dp_algo_dag_sequential = algorithm-figure.with(supplement: "Algoritmo")(
  "Tiled Knapsack DP",
  inset: 0.42em,
  {
    import algorithmic: *
    let AddVertex = Call.with("AddVertex")
    Procedure(
      "Costruzione DAG",
      ($n$, $C$, $w$, $v$, $"item_block"$, $"cap_block"$),
      {
        Comment[Inizializzazione]
        Assign[$"nb"$][$floor((n+"item_block"- 1)/ "item_block")$]
        Assign[$"nq"$][$floor((C+ "cap_block")/"cap_block")$]

        Comment[Costruzione del DAG, $"vertex"(b, q) = b dot "nq" + q$]
        For($b <- 1 space "to" space "nb"$, {
          For($q <- 0 space "to" space "nq" - 1$, {
            AddVertex[$(b - 1, q) -> (b, q)$]
            For($"item" space i space in space "blocco b"$, {
              Assign[$"lo"$][$min(q dot "cap_block" - w_i, 0)$]
              Assign[$"hi"$][$max((q + 1) dot "cap_block" - 1 - w_i, C)$]
              For($"column qp" in ("lo", "hi")$, {
                If(
                  $"qp" < "q"$,
                  {
                    AddVertex[$(b, "qp") -> (b, q)$]
                    AddVertex[$(b - 1, "qp") -> (b, q)$]
                  },
                )
              })
            })
          })
        })

        Procedure(
          "Risoluzione DAG",
          ($G$, $C$),
          {
            For($"tile" (b, q) space "in ordine row-major"$, {
              Assign[$"block"$][matrice della tile con $"rows" = "item_block" + 1$]
              If(
                $"b" == 0$,
                {
                  Assign[$"block"(0, c)$][0]
                },
                {
                  For($"ogni colonna" c$, {
                    Assign[$"wp"$][$q dot "cap_block" + c$]
                    Assign[$"block"(0, c)$][$"tile"(b - 1, "wp"/"cap_block")."block"("rows_local", "wp" mod "cap_block")$]
                  })
                },
              )
              For($"item" space a space "dove" space i = b dot "item_block" + a$, {
                For($"ogni colonna" c$, {
                  Assign[$"wp"$][$q dot "cap_block" + c$]
                  Assign[$"v"$][$"block"(a, c)$]
                  If(
                    $"w"_i <= "wp"$,
                    {
                      Assign[$"src"$][$"wp" - "w"_i$]
                      IfElseChain(
                        $"src" >= "lo"$,
                        {
                          Assign[$"candidate"$][$"block"(a, "src" - "lo") + v_i$]
                        },
                        {
                          Assign[$"candidate"$][$"tile"(b, "src"/"cap_block")."block"(a, "src" mod "cap_block") + v_i$]
                        },
                      )
                      Assign[$"v"$][$"max"("v", "candidate")$]
                    },
                  )
                  Assign[$"block"(a + 1, c)$][$v$]
                })
              })
            })
            Assign[$"opt"$][$"tile"("nb" - 1, "nq" - 1)."block"("rows" - 1, "C" - ("nq" - 1) dot "cap_block")$]
          },
        )
        Procedure("Backtrack items", ($"dp"$, $C$), {
          Comment[Ricostruzione degli item]
          Assign[$"items"$][$emptyset$]
          Assign[$"wp"$][$C$]
          For($"i" <- n space "downto" space 1$, {
            If(
              $"dp"(i, "wp") != "dp"(i - 1, "wp")$,
              {
                Assign[$"items"$][$"items" union {i - 1}$]
                Assign[$"wp"$][$"wp" - w[i - 1]$]
              },
            )
          })
          Return[$"items , opt"$]
        })
      },
    )
  },
);

#let KpDp = algorithm-figure.with(supplement: "Algoritmo")("Knapsack DP: parallelizzazione per riga", {
  import algorithmic: *
  Procedure("Knapsack DP", ("weights:[]", "values:[]", "C: int"), {
    Assign[$n$][$"len(weights)"$]
    Assign[$"dp"$][$"array"(n+1, C+1) "con tutte le celle azzerate"$]
    For($i in "(1..n)"$, {
      ParallelFor($w in "(0..C)"$, {
        IfElseChain(
          $"weights"[i-1] <= w$,
          {
            Assign[$"dp[i][w]"$][$"max(dp[i-1][w],dp[i-1][w-weights[i-1]] + values[i-1])"$]
          },
          {
            Assign[$"dp[i][w]"$][$"dp[i-1][w]"$]
          },
        )
      })
    })
  })
})

#let KpDpDag = algorithm-figure.with(supplement: "Algoritmo")(
  "KnapsackDPDAG",
  {
    import algorithmic: *
    let ww = "weights"
    let vv = "values"
    let cc = "capacity"
    let ib = "item_block"
    let cb = "cap_block"
    Procedure("KnapsackDPDAG", ("weights:[]", "values:[]", "capacity", "item_block", "cap_block"), {
      Assign[$g$][#CallInline("GenerateDAG", [$ww,cc,ib,cb$])]
      Assign[$n$][#CallInline[$"len"$][$ww$]]
      Comment[I vertici del DAG sono in ordine topologico]
      Assign[$"levels"$][#CallInline("AssignLevels", [$g$])]
      Assign[$L_"max"$][$max("level[v]" "for" v in g)$]
      For($v in g$, {
        Assign[$l$][$"level[v]"$]
        Call[Append][$"by_level[l]"$,$v$]
      })
      For($l in "(0..L_max)"$, {
        ParallelFor($v in "by_level[l]"$, {
          CallInline("ComputeTile", [$g, v, ww, vv, ib, cb$])
        })
      })
      Assign[$"last"$][$g[("nb" - 1) dot "nq" + "nq" - 1]$]
      Assign[$"opt"$][$"last.block"("last.rows" - 1, "capacity" - "last.lo")$]
      Return[$"opt"$]
    })
  },
)


#let ComputeDepRanges = algorithm-figure.with(supplement: "Algoritmo")(
  "KnapsackDPDAG.ComputeDepRanges",
  inset: (x: 0.2em, y: 0.42em),
  {
    import algorithmic: *
    Procedure("ComputeDepRanges", ("tile", "weights:[]", "capacity", "item_block", "cap_block"), {
      Assign[$n$][$"len(weights)"$]
      Assign[$b$][$"tile.b"$]
      Assign[$q$][$"tile.q"$]
      Assign[$"lo"$][$q dot "cap_block"$]
      Assign[$"hi"$][$min((q+1) dot "cap_block" - 1, "capacity")$]
      Assign[$D$][$emptyset$]
      For($i in "(b dot item_block + 1 .. min((b+1) dot item_block,n))"$, {
        Assign[$w_i$][$"weights"[i-1]$]
        Assign[$"d_lo"$][$max("lo" - w_i, 0)$]
        Assign[$"d_hi"$][$min("hi" - w_i, "capacity")$]
        If(
          $"d_lo" <= "d_hi"$,
          {
            Assign[$D$][$D union {("d_lo", "d_hi")}$]
          },
        )
      })
      Return[$D$]
    })
  },
)

#let GenerateDAG = algorithm-figure.with(supplement: "Algoritmo")(
  "KnapsackDPDAG.GenerateDAG",
  inset: (x: 0.2em, y: 0.42em),
  {
    import algorithmic: *
    let AddVertex = Call.with("AddVertex")
    let AddEdge = Call.with("AddEdge")
    Procedure("GenerateDAG", ("weights:[]", "capacity", "item_block", "cap_block"), {
      Assign[$n$][$"len(weights)"$]
      Assign[$"nb"$][$ceil(n/"item_block")$]
      Assign[$"nq"$][$ceil(("capacity"+1)/"cap_block")$]
      Assign[$g$][$"Graph{}"$]
      Comment[Fase 1: creazione dei vertici e degli intervalli in ordine row-major]
      For($b in "(0..nb-1)"$, {
        For($q in "(0..nq-1)"$, {
          AddVertex[$(b,q)$]
          Assign[$"dep_ranges"$][#CallInline("ComputeDepRanges", [$"tile"(b,q),"weights","capacity","item_block","cap_block"$])]
        })
      })
      Comment[Fase 2: creazione degli archi]
      For($b in "(0..nb-1)"$, {
        For($q in "(0..nq-1)"$, {
          If(
            $b >= 1$,
            {
              Comment[Dipendenza del caso skip]
              AddEdge[$(b-1,q) arrow (b,q)$]
            },
          )
          Comment[Dipendenze del caso take dai tile superiori e sinistri]
          Call[AddEdgesFromDependencyRanges][$g$, $(b,q)$, $"dep_ranges"$]
        })
      })
      Return[$g$]
    })
  },
)

#let AssignLevels = algorithm-figure.with(supplement: "Algoritmo")(
  "KnapsackDPDAG.AssignLevels",
  inset: (x: 0.2em, y: 0.42em),
  {
    import algorithmic: *
    Procedure("KnapsackDPDAG.AssignLevels", "g", {
      Comment[Row-major: quando si visita v, tutti i predecessori hanno livello già assegnato]
      For($v in g$, {
        IfElseChain(
          $"Pred(v)" = emptyset$,
          {
            Assign["level[v]"][0]
          },
          {
            Assign["level[v]"][$max("p.level" in "Pred"(v)) + 1$]
          },
        )
      })
      Return[$"level"$]
    })
  },
)

#let ComputeTile = algorithm-figure.with(supplement: "Algoritmo")(
  "KnapsackDPDAG.ComputeTile",
  inset: (x: 0.2em, y: 0.42em),
  {
    import algorithmic: *
    Procedure("KnapsackDPDAG.ComputeTile", ("g", "tile", "weights:[]", "values:[]", "item_block", "cap_block"), {
      Assign[$"lo"$][$"tile.lo"$]
      Assign[$"rows_local"$][$"tile.rows" - 1$]
      Assign[$"width"$][$"tile.width"$]
      Comment[La riga 0 del blocco è la riga di confine]
      IfElseChain(
        $"tile.b" = 0$,
        {
          For($c in "(0..width-1)"$, {
            Assign[$"tile.block"(0,c)$][$0$]
          })
        },
        {
          For($c in "(0..width-1)"$, {
            Assign[$"wp"$][$"lo" + c$]
            Assign[$"qp"$][$floor("wp"/"cap_block")$]
            Assign[$"c_prev"$][$"wp" - "qp" dot "cap_block"$]
            Assign[$"src"$][$g[("tile.b" - 1) dot "nq" + "qp"]$]
            Assign[$"tile.block"(0,c)$][$"src.block"("src.rows" - 1, "c_prev")$]
          })
        },
      )
      Comment[Compute degli item locali]
      For($a in "(0..rows_local-1)"$, {
        Assign[$i$][$"tile.b" dot "item_block" + a + 1$]
        Assign[$w_i$][$"weights"[i-1]$]
        Assign[$v_i$][$"values"[i-1]$]
        Assign[$"c_fit"$][$max(min(w_i - "lo", "width"), 0)$]
        Assign[$"c_in"$][$max(min(w_i, "width"), "c_fit")$]
        Comment[L'item non entra nella capacità corrente]
        For($c in "(0..c_fit-1)"$, {
          Assign[$"tile.block"(a+1,c)$][$"tile.block"(a,c)$]
        })
        Comment[La sorgente del caso take appartiene a un tile a sinistra]
        For($c in "(c_fit..c_in-1)"$, {
          Assign[$"src_c"$][$"lo" + c - w_i$]
          Assign[$"qp"$][$floor("src_c"/"cap_block")$]
          Assign[$"c_prev"$][$"src_c" - "qp" dot "cap_block"$]
          Assign[$"left"$][$g["tile.b" dot "nq" + "qp"]$]
          Assign[$"take"$][$"left.block"(a, "c_prev") + v_i$]
          Assign[$"tile.block"(a+1,c)$][$max("tile.block"(a,c), "take")$]
        })
        Comment[La sorgente del caso take è interna al tile]
        For($c in "(c_in..width-1)"$, {
          Assign[$"take"$][$"tile.block"(a, c - w_i) + v_i$]
          Assign[$"tile.block"(a+1,c)$][$max("tile.block"(a,c), "take")$]
        })
      })
    })
  },
)
