#import "@preview/algorithmic:1.0.7"
#import algorithmic: Line, algorithm-figure, style-algorithm
#import "@preview/codelst:2.0.2": sourcecode

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
#let reqline(body) = Line(text(body, style: "italic"))
#let ensureline(body) = Line(text(body, style: "italic"))
#let states(..parts) = Line(parts.pos().join(","))


