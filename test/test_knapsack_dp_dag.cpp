#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackdpdag_impl.hpp"
#include <algorithm>
#include <random>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

TEST(KnapsackDPDAG, TiledSolverMatchesClassicDP)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	const auto classic = knapsackdp(weights, values, capacity);

	for (int item_block : {1, 2, 4, 10})
	{
		for (int cap_block : {1, 3, 7})
		{
			auto g = build_graph(weights, capacity, item_block, cap_block);
			const int best = solve_dag(g, weights, values, capacity, item_block, cap_block);
			EXPECT_EQ(best, classic.totalValue) << "item_block=" << item_block << " cap_block=" << cap_block;
		}
	}
}

TEST(KnapsackDPDAG, PublicInterfaceMatchesClassicOnRandomInstances)
{
	std::mt19937 rng(7);
	for (int trial = 0; trial < 5; ++trial)
	{
		const int n = 1 + static_cast<int>(rng() % 40);
		std::vector<int> weights(n), values(n);
		const int capacity = 20 + static_cast<int>(rng() % 200);
		for (int i = 0; i < n; ++i)
		{
			weights[i] = 1 + static_cast<int>(rng() % 100);
			values[i] = 1 + static_cast<int>(rng() % 10);
		}

	const auto classic = knapsackdp(weights, values, capacity);
		const auto dag = knapsackdpdag(weights, values, capacity);

		EXPECT_EQ(dag.totalValue, classic.totalValue) << "trial=" << trial;
		EXPECT_LE(dag.totalWeight, capacity) << "trial=" << trial;
	}
}

// Verifies that build_graph populates, on every vertex, the embedded tile
// geometry fields (lo, hi, width) and the per-item capacity dependency ranges
// (dep_ranges), and that the DAG edges are consistent with those ranges.
TEST(KnapsackDPDAG, NodeDataEmbedsCapacityRanges)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	for (int item_block : {1, 2, 4, 10})
	{
		for (int cap_block : {1, 3, 7})
		{
			auto g = build_graph(weights, capacity, item_block, cap_block);

			const int n = static_cast<int>(weights.size());
			const int nb = (n + item_block - 1) / item_block;
			const int nq = (capacity + cap_block) / cap_block;

			for (int b = 0; b < nb; ++b)
			{
				const int i_start = b * item_block;
				const int i_end = std::min((b + 1) * item_block, n);
				const int rows_local = i_end - i_start;
				for (int q = 0; q < nq; ++q)
				{
					const auto v = static_cast<std::size_t>(b) * nq + q;
					const NodeData &nd = g[v];

					// 1. Geometry of the embedded tile coverage.
					EXPECT_EQ(nd.lo, q * cap_block) << "b=" << b << " q=" << q << " cap_block=" << cap_block;
					EXPECT_EQ(nd.hi, nd.lo + nd.width - 1) << "b=" << b << " q=" << q << " cap_block=" << cap_block;
					EXPECT_EQ(nd.width, std::min(cap_block, capacity + 1 - nd.lo))
						<< "b=" << b << " q=" << q << " cap_block=" << cap_block;

					// 2. Per-item dependency ranges.
					EXPECT_EQ(nd.dep_ranges.size(), static_cast<std::size_t>(rows_local))
						<< "b=" << b << " q=" << q << " cap_block=" << cap_block;
					for (int a = 0; a < rows_local; ++a)
					{
						const int wi = weights[i_start + a];
						const int dlo = nd.lo - wi;
						const int dhi = (q + 1) * cap_block - 1 - wi;
						const auto &dep = nd.dep_ranges[static_cast<std::size_t>(a)];
						if (dhi < 0 || dlo > capacity)
						{
							// Empty marker {1,0}: first > second.
							EXPECT_GT(dep.first, dep.second)
								<< "b=" << b << " q=" << q << " a=" << a << " cap_block=" << cap_block
								<< " item_block=" << item_block;
						}
						else
						{
							const int exp_lo = std::max(dlo, 0);
							const int exp_hi = std::min(dhi, capacity);
							EXPECT_EQ(dep.first, exp_lo)
								<< "b=" << b << " q=" << q << " a=" << a << " cap_block=" << cap_block
								<< " item_block=" << item_block;
							EXPECT_EQ(dep.second, exp_hi)
								<< "b=" << b << " q=" << q << " a=" << a << " cap_block=" << cap_block
								<< " item_block=" << item_block;
						}

						// 3. Edge-overlap consistency for non-empty ranges.
						if (dep.first <= dep.second)
						{
							const int dlo_ij = dep.first;
							const int dhi_ij = dep.second;
							// Capacity-blocks in the row above overlapping [dlo_ij, dhi_ij].
							if (b >= 1)
							{
								for (int qp = 0; qp < nq; ++qp)
								{
									if (qp * cap_block <= dhi_ij && (qp + 1) * cap_block - 1 >= dlo_ij)
									{
										const auto src = static_cast<std::size_t>(b - 1) * nq + qp;
										const auto [e, exists] = boost::edge(src, v, g);
										(void)e;
										EXPECT_TRUE(exists)
											<< "missing edge (" << (b - 1) << "," << qp << ") -> (" << b << "," << q
											<< ") cap_block=" << cap_block << " item_block=" << item_block;
									}
								}
							}
							// Same-row capacity-blocks to the left overlapping [dlo_ij, dhi_ij].
							for (int qp = 0; qp < q; ++qp)
							{
								if (qp * cap_block <= dhi_ij && (qp + 1) * cap_block - 1 >= dlo_ij)
								{
									const auto src = static_cast<std::size_t>(b) * nq + qp;
									const auto [e, exists] = boost::edge(src, v, g);
									(void)e;
									EXPECT_TRUE(exists)
										<< "missing edge (" << b << "," << qp << ") -> (" << b << "," << q
										<< ") cap_block=" << cap_block << " item_block=" << item_block;
								}
							}
						}
					}
				}
			}
		}
	}
}

TEST(KnapsackDPDAG, TopoSolverMatchesLevelized)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	const auto classic = knapsackdp(weights, values, capacity);

	for (int item_block : {1, 2, 4, 10})
	{
		for (int cap_block : {1, 3, 7})
		{
			auto g1 = build_graph(weights, capacity, item_block, cap_block);
			const int levelized = solve_dag(g1, weights, values, capacity, item_block, cap_block);

			auto g2 = build_graph(weights, capacity, item_block, cap_block);
			const int topo_values = solve_dag_topo(g2, weights, values, capacity, item_block, cap_block);

			EXPECT_EQ(topo_values, levelized) << "item_block=" << item_block << " cap_block=" << cap_block;
			EXPECT_EQ(topo_values, classic.totalValue) << "item_block=" << item_block << " cap_block=" << cap_block;

			const auto sol = knapsackdpdag(weights, values, capacity, item_block, cap_block);
			EXPECT_EQ(sol.totalValue, topo_values) << "item_block=" << item_block << " cap_block=" << cap_block;
		}
	}
}
