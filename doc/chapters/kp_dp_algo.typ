
#import "@preview/algorithmic:1.0.7"
#import algorithmic: Line, algorithm-figure, style-algorithm
#import "@preview/codelst:2.0.2": sourcecode

#show: style-algorithm
#algorithm-figure(
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
            Comment[l'ordine row-major $(b, q)$ è un ordinamento topologico valido]

            Comment[Risoluzione per tile in ordine topologico]
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
  supplement: none,
) <alg:tiled_dp>

