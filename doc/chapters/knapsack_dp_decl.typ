#import "@preview/algorithmic:1.0.7"
#import algorithmic: algorithm-figure, style-algorithm
#import "@preview/codelst:2.0.2": sourcecode


Spiegare qui come è divisa la DP e cosa sono le tile B,Q

#show: style-algorithm
#algorithm-figure(
  "Tiled Knapsack Dp ",
  vstroke: .10pt + luma(200),
  inset: 0.4em,

  {
    import algorithmic: *
    let AddArch = Call.with("AddArch")

    Procedure("Tiled Knapsack DP", ("w: []", "v: [],  C: int, item_block: int, cap_block: int"), {
      Comment(
        text("w-> lista di pesi, v-> lista di valori"),
      )
      Comment(text("item_block-> numero di elementi nel tile (righe nel tile)"))
      Comment(text("cap_block -> numero di colonne capacità (colonne nel tile )"))
      Assign[$n$][$"len"(w)$ ]
      Assign[$"nb"$][$ceil((n/ "item_block"))$]
      Assign[nq][$ceil((C+1)/ "cap_block")$]
      Comment(text("Costruzione del DAG"))
      For($"tile" in (b,q) "with" b >= 1$, {
        AddArch[$(b-1,q) -> (b,q)$]
        For($"item i" in "block b"$, {
          Assign[$"lo"$][$q*"cap_block"-w_i$]
        })
      })
    })
  },
)

