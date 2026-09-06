#include "knapsack.hpp"
#include "knapsackdpdag_impl.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <time.hpp>

// File-scope stats for the most recent DAG solve (single-threaded CLI use).
namespace
{
DAGStats g_last_dag_stats{};

// A/B switch: use the topological scheduler by default; override with the
// KNAPSACK_DAG_TOPO environment variable ("0" selects the levelized solver).
bool use_topo_scheduler()
{
	static const bool topo = []() {
		const char *env = std::getenv("KNAPSACK_DAG_TOPO");
		if (env == nullptr)
			return true;
		return std::strcmp(env, "0") != 0;
	}();
	return topo;
}
} // namespace

DAGStats get_dag_stats()
{
	return g_last_dag_stats;
}

Graph build_graph(const std::vector<int> &weights, int capacity, int item_block, int cap_block)
{
	const int n = static_cast<int>(weights.size());
	const int nb = (n + item_block - 1) / item_block;
	const int nq = (capacity + cap_block) / cap_block;

	Graph g;

	// Add all vertices first (row-major), so the vertex descriptor is b*nq+q.

	for (int b = 0; b < nb; ++b)
	{
		const int i_start = b * item_block;
		const int i_end = std::min((b + 1) * item_block, n);
		const int rows_local = i_end - i_start;
		for (int q = 0; q < nq; ++q)
		{
			const int lo = q * cap_block;
			const int width = std::min(cap_block, capacity + 1 - q * cap_block);
			const int hi = lo + width - 1;
			// Every cell of the block is overwritten by solve_dag before it is
			// read (row 0 for all c, then rows a+1 for all c), so skip zeroing.
			NodeData nd{b, q, rows_local + 1, width, 0, lo, hi, {}, BlockMatrix(rows_local + 1, width)};

			// Per-item dependency range: the capacity interval read from the
			// source row when taking local item a. Empty marker {1,0} when the
			// item never fits in this tile's capacity range.
			nd.dep_ranges.reserve(static_cast<std::size_t>(rows_local));
			for (int a = 0; a < rows_local; ++a)
			{
				const int wi = weights[i_start + a];
				const int dlo = lo - wi;
				const int dhi = (q + 1) * cap_block - 1 - wi;
				if (dhi < 0 || dlo > capacity)
					nd.dep_ranges.emplace_back(1, 0);
				else
					nd.dep_ranges.emplace_back(std::max(dlo, 0), std::min(dhi, capacity));
			}

			const auto v = boost::add_vertex(g);
			g[v] = std::move(nd);
		}
	}

	for (int b = 0; b < nb; ++b)
	{
		const int i_start = b * item_block;
		const int i_end = std::min((b + 1) * item_block, n);
		const int rows_local = i_end - i_start;
		for (int q = 0; q < nq; ++q)
		{
			const auto dst = static_cast<std::size_t>(b) * nq + q;

			// Straight-above tile in the previous item-block.
			if (b >= 1)
				boost::add_edge(static_cast<std::size_t>(b - 1) * nq + q, dst, g);

			// Gather, sort and merge the per-item dependency intervals so each
			// source edge is added at most once (strict dedup, edge set/levels
			// unchanged). The edge set spans the same [loM, hiM] range as the
			// union of all per-item intervals.
			std::vector<std::pair<int, int>> merged;
			for (int a = 0; a < rows_local; ++a)
			{
				const auto [lo_ij, hi_ij] = g[dst].dep_ranges[a];
				if (lo_ij <= hi_ij)
					merged.emplace_back(lo_ij, hi_ij);
			}
			std::sort(merged.begin(), merged.end());
			std::vector<std::pair<int, int>> merged2;
			for (const auto &[lo_i, hi_i] : merged)
			{
				if (!merged2.empty() && lo_i <= merged2.back().second + 1)
					merged2.back().second = std::max(merged2.back().second, hi_i);
				else
					merged2.push_back({lo_i, hi_i});
			}
			merged = std::move(merged2);

			for (const auto &[loM, hiM] : merged)
			{
				const int qp_lo = loM / cap_block;
				const int qp_hi = hiM / cap_block;

				// Capacity-blocks in the row above overlapping [loM, hiM].
				if (b >= 1)
				{
					for (int qp = qp_lo; qp <= qp_hi && qp < nq; ++qp)
						boost::add_edge(static_cast<std::size_t>(b - 1) * nq + qp, dst, g);
				}
				// Same-row capacity-blocks to the left overlapping [loM, hiM].
				for (int qp = qp_lo; qp <= qp_hi && qp < q; ++qp)
					boost::add_edge(static_cast<std::size_t>(b) * nq + qp, dst, g);
			}
		}
	}

	return g;
}

void compute_levels(Graph &g)
{
	const auto [vp, ve] = boost::vertices(g);
	for (auto it = vp; it != ve; ++it)
	{
		const auto v = *it;
		int level = 0;
		for (auto e : boost::make_iterator_range(boost::in_edges(v, g)))
		{
			const auto u = boost::source(e, g);
			level = std::max(level, g[u].level + 1);
		}
		g[v].level = level;
	}
}

/**
 * @brief Compute one tile's DP block in place.
 *
 * Reads the tile-above and left-neighbor tiles exactly as the DAG edges
 * describe and fills `g[idx].block`. Invoked by BOTH the levelized solver and
 * the topological scheduler with identical logic so the results match
 * bit-for-bit.
 */
void compute_tile(Graph &g, int idx, const std::vector<int> &weights, const std::vector<int> &values, int item_block,
				  int cap_block)
{
	// nb item-blocks x nq capacity-blocks fills the whole vertex set exactly,
	// so nq = num_vertices / nb (nb from the item count and item_block).
	const int nb = (static_cast<int>(weights.size()) + item_block - 1) / item_block;
	const int nq = (nb == 0) ? 0 : static_cast<int>(boost::num_vertices(g)) / nb;
	const int b = idx / nq;
	const int q = idx % nq;
	const int i_start = b * item_block;
	NodeData &nd = g[static_cast<std::size_t>(idx)];
	const int rows = nd.rows;
	const int width = nd.width;
	const int rows_local = rows - 1;

	// Boundary row 0: inherited from the tile above (or all zeros if b==0).
	for (int c = 0; c < width; ++c)
	{
		if (b == 0)
		{
			nd.block(0, c) = 0;
		}
		else
		{
			const int wp = nd.lo + c;
			const int q_prev = wp / cap_block;
			const int c_prev = wp - q_prev * cap_block;
			const NodeData &src = g[static_cast<std::size_t>(b - 1) * nq + q_prev];
			nd.block(0, c) = src.block(src.rows - 1, c_prev);
		}
	}

	// Interior rows: classic skip/take recurrence.
	for (int a = 0; a < rows_local; ++a)
	{
		const int i = i_start + a;
		const int wi = weights[i];
		const int pi = values[i];
		const int lo = nd.lo;
		for (int c = 0; c < width; ++c)
		{
			const int wp = lo + c;
			// "Skip item": carry previous best value.
			int v = nd.block(a, c);
			if (wi <= wp)
			{
				const int src = wp - wi;
				if (src >= lo)
				{
					// Source capacity is inside this same tile.
					v = std::max(v, nd.block(a, src - lo) + pi);
				}
				else
				{
					// Source capacity lies in a tile to the left (same row a).
					const int q_prev = src / cap_block;
					const int c_prev = src - q_prev * cap_block;
					const NodeData &left = g[static_cast<std::size_t>(b) * nq + q_prev];
					v = std::max(v, left.block(a, c_prev) + pi);
				}
			}
			nd.block(a + 1, c) = v;
		}
	}
}

int solve_dag(Graph &g, const std::vector<int> &weights, const std::vector<int> &values, int capacity, int item_block,
			  int cap_block)
{
	const int n = static_cast<int>(weights.size());
	const int nb = (n + item_block - 1) / item_block;
	const int nq = (capacity + 1 + cap_block - 1) / cap_block;

	if (nb == 0 || nq == 0)
		return 0;

	// Assign each tile its longest-path level.
	// Tiles with the same level have no dependency path between them
	// and can therefore be evaluated in parallel.
	compute_levels(g);

	int max_level = 0;
	for (int b = 0; b < nb; ++b)
		for (int q = 0; q < nq; ++q)
			max_level = std::max(max_level, g[static_cast<std::size_t>(b) * nq + q].level);

	std::vector<std::vector<int>> by_level(static_cast<std::size_t>(max_level) + 1);
	for (int b = 0; b < nb; ++b)
		for (int q = 0; q < nq; ++q)
		{
			const int lvl = g[static_cast<std::size_t>(b) * nq + q].level;
			by_level[static_cast<std::size_t>(lvl)].push_back(b * nq + q);
		}

	for (int L = 0; L <= max_level; ++L)
	{
		START_BLOCK("KnapsackDPDAG::LevelCompute");
		const std::vector<int> &tiles = by_level[static_cast<std::size_t>(L)];
#pragma omp parallel for
		for (std::size_t t = 0; t < tiles.size(); ++t)
		{
			compute_tile(g, tiles[t], weights, values, item_block, cap_block);
		}

		END_BLOCK("KnapsackDPDAG::LevelCompute");
	}

	const NodeData &last = g[static_cast<std::size_t>(nb - 1) * nq + (nq - 1)];
	const int c_last = capacity - last.lo;
	return last.block(last.rows - 1, c_last);
}

std::vector<int> reconstruct_items(const Graph &g, const std::vector<int> &weights, const std::vector<int> &values,
								   int capacity, int item_block, int cap_block)
{
	const int n = static_cast<int>(weights.size());
	const int nb = (n + item_block - 1) / item_block;
	const int nq = (capacity + 1 + cap_block - 1) / cap_block;

	// Read dp[i][w] from the tile grid.
	const auto dp_at = [&](int i, int w) -> int {
		if (i == 0)
			return 0;
		const int b = (i - 1) / item_block;
		const int q = w / cap_block;
		const NodeData &nd = g[static_cast<std::size_t>(b) * nq + q];
		const int r = i - b * item_block;
		const int c = w - q * cap_block;
		return nd.block(r, c);
	};

	std::vector<int> items;
	int w = capacity;
	int totalValue = dp_at(n, capacity);
	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp_at(i - 1, w))
		{
			items.push_back(i - 1);
			totalValue -= values[i - 1];
			w -= weights[i - 1];
		}
	}
	return items;
}

int solve_dag_topo(Graph &g, const std::vector<int> &weights, const std::vector<int> &values, int capacity,
				   int item_block, int cap_block)
{
	const int n = static_cast<int>(weights.size());
	const int nb = (n + item_block - 1) / item_block;
	const int nq = (capacity + 1 + cap_block - 1) / cap_block;
	const std::size_t V = boost::num_vertices(g);

	if (nb == 0 || nq == 0 || V == 0)
		return 0;

	// 1. Per-vertex in-degrees (edges are already deduplicated by build_graph).
	std::vector<std::atomic<int>> indeg(V);
	for (std::size_t v = 0; v < V; ++v)
		indeg[v].store(static_cast<int>(boost::in_degree(v, g)));

	// 2. Number of remaining tiles that must still be computed.
	std::atomic<int> remaining{static_cast<int>(V)};

	// 3. Shared ready stack: a fixed-size slot array indexed by an atomic
	//    head (next slot to claim) plus two allocation counters:
	//      * tail   - number of slots reserved by producers (handed out)
	//      * filled - number of slots whose value has been written & published
	//    A producer reserves a slot (tail.fetch_add), writes ready[slot], then
	//    publishes it with a release store to `filled`. A consumer that observes
	//    head < filled is guaranteed (by release/acquire) to see ready[head].
	std::vector<int> ready(V);
	std::atomic<std::size_t> head{0};
	std::atomic<std::size_t> tail{0};
	std::atomic<std::size_t> filled{0};
	auto push = [&](int v) {
		const std::size_t slot = tail.fetch_add(1, std::memory_order_relaxed);
		ready[slot] = v;
		filled.store(slot + 1, std::memory_order_release);
	};

	// Seed with all in-degree-zero vertices, in row-major (vertex) order.
	for (std::size_t v = 0; v < V; ++v)
		if (indeg[v].load(std::memory_order_relaxed) == 0)
			push(static_cast<int>(v));

#pragma omp parallel
	{
		while (true)
		{
			if (remaining.load(std::memory_order_acquire) == 0)
				break; // every tile computed -> queue is drained and safe to stop

			const std::size_t h = head.load(std::memory_order_acquire);
			const std::size_t f = filled.load(std::memory_order_acquire);
			if (h < f)
			{
				// Try to claim slot h.
				std::size_t expected = h;
				if (!head.compare_exchange_strong(expected, h + 1, std::memory_order_acq_rel))
					continue; // lost the race; retry the loop

				const int idx = ready[h]; // slot h is published (filled > h => write is visible)
				compute_tile(g, idx, weights, values, item_block, cap_block);

				// 5. Decrement each successor's in-degree; push the last one.
				for (auto e : boost::make_iterator_range(boost::out_edges(idx, g)))
				{
					const int u = static_cast<int>(boost::target(e, g));
					if (indeg[u].fetch_sub(1, std::memory_order_acq_rel) == 1)
						push(u);
				}

				// 6. This tile is fully done.
				remaining.fetch_sub(1, std::memory_order_acq_rel);
			}
			else
			{
				// Queue temporarily empty but work still outstanding: bounded spin.
				for (int s = 0; s < 64; ++s)
					std::this_thread::yield();
			}
		}
	}

	// Bottom-right tile holds dp[n][capacity].
	const NodeData &last = g[static_cast<std::size_t>(nb - 1) * nq + (nq - 1)];
	const int c_last = capacity - last.lo;
	return last.block(last.rows - 1, c_last);
}

KnapsackSolution knapsackdpdag(const std::vector<int> &weights, const std::vector<int> &values, int capacity,
							   int item_block, int cap_block)
{
	const int n = static_cast<int>(weights.size());
	if (cap_block == 0)
		cap_block = std::max(1, (capacity + 1) / 10);
	const int nb = (n + item_block - 1) / item_block;
	const int nq = (capacity + 1 + cap_block - 1) / cap_block;

	START_BLOCK("GraphBuild");
	Graph g = build_graph(weights, capacity, item_block, cap_block);
	END_BLOCK("GraphBuild");
	START_BLOCK("Dag Solve");
	const int bestValue = use_topo_scheduler() ? solve_dag_topo(g, weights, values, capacity, item_block, cap_block)
											   : solve_dag(g, weights, values, capacity, item_block, cap_block);
	END_BLOCK("Dag Solve");

	// DAG statistics: compute levels serially (outside any parallel region)
	// and record tiles/edges/levels/maxFrontier for the Python report.
	compute_levels(g);
	int max_level = 0;
	for (int b = 0; b < nb; ++b)
		for (int q = 0; q < nq; ++q)
			max_level = std::max(max_level, g[static_cast<std::size_t>(b) * nq + q].level);
	std::vector<int> level_freq(static_cast<std::size_t>(max_level) + 1, 0);
	for (int b = 0; b < nb; ++b)
		for (int q = 0; q < nq; ++q)
			++level_freq[static_cast<std::size_t>(g[static_cast<std::size_t>(b) * nq + q].level)];
	int max_frontier = 0;
	for (std::size_t L = 0; L < level_freq.size(); ++L)
		max_frontier = std::max(max_frontier, level_freq[L]);
	g_last_dag_stats.tiles = nb * nq;
	g_last_dag_stats.edges = static_cast<int>(boost::num_edges(g));
	g_last_dag_stats.levels = max_level + 1;
	g_last_dag_stats.maxFrontier = max_frontier;

	const std::vector<int> items = reconstruct_items(g, weights, values, capacity, item_block, cap_block);

	int totalWeight = 0;
	for (int idx : items)
		totalWeight += weights[idx];

	return {items, bestValue, totalWeight};
}
