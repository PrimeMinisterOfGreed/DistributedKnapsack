#pragma once
#include <Eigen/Dense>
#include <boost/graph/adjacency_list.hpp>
#include <vector>

/** @brief Row-major integer matrix used for a tile's DP block. */
using BlockMatrix = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

/**
 * @brief Per-vertex (per-tile) data attached to the dependency DAG.
 *
 * Each tile (b, q) covers a rectangular slice of the classic DP table:
 *   - items  [b*item_block, min((b+1)*item_block, n))   -> `rows` rows
 *   - capacities [q*cap_block, q*cap_block + `width`)   -> `width` columns
 *
 * The DP block is an Eigen matrix with `rows` rows and `width` columns
 * (row-major, so block(r, c) is the cell for item r and capacity offset c),
 * where row 0 is the boundary value inherited from the tile above.
 */
struct NodeData
{
	int b, q;		 // tile grid coordinates
	int rows;		 // rows_local + 1 (number of rows in this tile's block)
	int width;		 // wq (number of columns in this tile's block)
	int level;		 // longest-path wavefront level (for future parallelization)
	BlockMatrix block; // DP block, dimensions rows x width
};

namespace _detail
{
using namespace boost;
using _Graph = boost::adjacency_list<vecS, vecS, bidirectionalS, property<vertex_index_t, int, NodeData>>;
} // namespace _detail

using Graph = _detail::_Graph;

/**
 * @brief Build the tile dependency DAG over the (item_block x cap_block) grid.
 *
 * Vertices are created in row-major order, so the vertex descriptor of tile
 * (b, q) is `b * nq + q` (where `nq` is the number of capacity blocks). Each
 * vertex carries an allocated NodeData block. Edges encode the dependencies a
 * tile needs before it can be computed.
 *
 * @param weights  item weights (size n)
 * @param capacity knapsack capacity
 * @param item_block items per tile row
 * @param cap_block  capacities per tile column
 * @return the built Graph
 */
Graph build_graph(const std::vector<int> &weights, int capacity, int item_block, int cap_block);

/**
 * @brief Assign longest-path levels over the DAG (for parallel wavefronts).
 *
 * Relies on the fact that vertices are already in topological (row-major)
 * order, so each vertex's predecessors have smaller indices and are already
 * level-assigned. Vertices sharing the same level are mutually independent.
 *
 * @param g the graph whose `level` fields will be filled
 */
void compute_levels(Graph &g);

/**
 * @brief Fill every tile's DP block in topological order.
 *
 * Returns the optimal value, i.e. dp[n][capacity], found at the bottom-right
 * tile.
 *
 * @param g the DAG built by build_graph (block buffers are written here)
 * @param weights item weights
 * @param values  item values
 * @param capacity knapsack capacity
 * @param item_block items per tile row
 * @param cap_block  capacities per tile column
 * @return the optimal total value
 */
int solve_dag(Graph &g, const std::vector<int> &weights, const std::vector<int> &values, int capacity, int item_block,
			  int cap_block);

/**
 * @brief Recover the indices of the items included in the optimal solution.
 *
 * Reads the per-tile DP blocks (filled by solve_dag) to backtrack from
 * dp[n][capacity], mirroring the classic DP backtracking.
 *
 * @param g the DAG filled by solve_dag
 * @param weights item weights
 * @param values  item values
 * @param capacity knapsack capacity
 * @param item_block items per tile row
 * @param cap_block  capacities per tile column
 * @return the indices of the selected items (0-indexed)
 */
std::vector<int> reconstruct_items(const Graph &g, const std::vector<int> &weights, const std::vector<int> &values,
								   int capacity, int item_block, int cap_block);
