import argparse
import random
from math import ceil


def parse_args():
    """Parse command-line arguments for the knapsack instance and tile sizes."""
    parser = argparse.ArgumentParser()
    parser.add_argument("n", nargs="?", type=int, default=100, help="number of items")
    parser.add_argument("--capacity", type=int, default=10000, help="knapsack capacity")
    parser.add_argument("--item-block", type=int, default=10, help="items per tile row")
    parser.add_argument("--cap-block", type=int, default=None, help="capacities per tile column")
    args = parser.parse_args()
    return args.n, args.capacity, args.item_block, args.cap_block


def build_dp(num_items, capacity, weights, values):
    """Classic 0/1 knapsack dynamic-programming table (reference implementation).

    Returns dp[i][wp] = best total value using the first i items with capacity wp.
    """
    # dp[i][wp] = best total value using the first i items with a capacity of wp.
    # 2D table of size (num_items+1) x (capacity+1), all zeros by default.
    dp = [[0] * (capacity + 1) for _ in range(num_items + 1)]
    for i in range(1, num_items + 1):  # consider items one at a time
        wi, pi = weights[i - 1], values[i - 1]  # weight and value of item i (0-indexed access)
        for wp in range(0, capacity + 1):  # every possible capacity value
            # Case 1: skip the item, keep the best value without it
            dp[i][wp] = dp[i - 1][wp]
            if wi <= wp:
                # Case 2: take the item (fits), value = best with leftover capacity + its value
                cand = dp[i - 1][wp - wi] + pi
                if cand > dp[i][wp]:
                    dp[i][wp] = cand
    return dp


def solve_dag_tiled(num_items, capacity, weights, values, item_block, cap_block, num_item_blocks,
                    num_cap_blocks):
    """Solve the knapsack using tiled DP over a tile grid.

    The grid is split into item-blocks (rows) and capacity-blocks (columns). Each
    tile (b, q) holds a DP sub-table restricted to its item range and capacity
    range. Returns the map of tiles and per-tile read/write counters.
    """
    # tiles[(b, q)] holds the DP block for item-block b and capacity-block q.
    # Each block is a list of rows: row a (0..rows_local) is dp for the state
    # after processing the first a items of the block, restricted to this capacity range.
    tiles = {}
    # --- [CONTEggio per tile - evidenziato: togliere se non serve] -----------
    # stats[(b, q)] counts how many cells are read/written per tile, for work measurement.
    stats = {(b, q): {"reads": 0, "writes": 0} for b in range(num_item_blocks) for q in range(num_cap_blocks)}
    # -------------------------------------------------------------------------
    for b in range(num_item_blocks):
        # Indices of the items belonging to this item-block.
        i_start = b * item_block
        i_end = min((b + 1) * item_block, num_items)
        # Number of items in this block (last block may be shorter).
        rows_local = i_end - i_start
        block_rows = {}
        for q in range(num_cap_blocks):
            # Width (in columns) of this capacity-block: full block, or the leftover tail.
            wq = min(cap_block, capacity + 1 - q * cap_block)
            # Allocate the block: (rows_local + 1) rows of wq cells (row 0 = boundary).
            block_rows[(b, q)] = [[0] * wq for _ in range(rows_local + 1)]
        if b == 0:
            # First item-block: no predecessor block, so row 0 is all zeros (dp boundary).
            for q in range(num_cap_blocks):
                block_rows[(b, q)][0] = [0] * len(block_rows[(b, q)][0])
        else:
            # Row 0 of the block = last row (row rows_local) of the predecessor block
            # in the item-block above, at the matching capacity column.
            for q in range(num_cap_blocks):
                wq = len(block_rows[(b, q)][0])
                for c_local in range(wq):
                    # Absolute capacity this local column represents.
                    wp = q * cap_block + c_local
                    # Map wp into the predecessor grid: which block, and which column inside it.
                    q_prev = wp // cap_block
                    c_local_prev = wp - q_prev * cap_block
                    # Copy the boundary value from the row above.
                    block_rows[(b, q)][0][c_local] = \
                        tiles[(b - 1, q_prev)][-1][c_local_prev]
                    # --- [CONTEggio - evidenziato] ---
                    stats[(b, q)]["reads"] += 1
                    # ----------------------------------
        # Fill the interior rows of every capacity-block for this item-block.
        for a in range(rows_local):
            # i = actual item index, its weight and value.
            i = i_start + a
            wi, pi = weights[i], values[i]
            for q in range(num_cap_blocks):
                # lo = absolute capacity of the first column of this block.
                lo = q * cap_block
                wq = len(block_rows[(b, q)][0])
                prev = block_rows[(b, q)][a]  # row before adding item a
                nxt = block_rows[(b, q)][a + 1]  # row after adding item a (to fill)
                for c_local in range(wq):
                    wp = lo + c_local  # absolute capacity of this column
                    # Start from "skip item": carry the previous best value.
                    v = prev[c_local]
                    stats[(b, q)]["reads"] += 1
                    if wi <= wp:
                        # "Take item" candidate: best value at capacity (wp - wi), plus its value.
                        src = wp - wi
                        if src >= lo:
                            # The source capacity is inside this same block.
                            cand = prev[src - lo] + pi
                            stats[(b, q)]["reads"] += 1
                        else:
                            # The source capacity lies in a block to the left;
                            # read from the already-computed row a of that block.
                            q_prev = src // cap_block
                            c_local_prev = src - q_prev * cap_block
                            cand = block_rows[(b, q_prev)][a][c_local_prev] + pi
                            stats[(b, q)]["reads"] += 1
                            stats[(b, q_prev)]["reads"] += 1
                        if cand > v:
                            v = cand
                    nxt[c_local] = v
                    stats[(b, q)]["writes"] += 1
        tiles.update(block_rows)
    return tiles, stats


def build_graph(num_items, capacity, weights, item_block, cap_block):
    """Build the dependency DAG of tiles over the item/capacity block grid.

    Returns (num_item_blocks, num_cap_blocks, edges) where edges[(b, q)] is the
    set of predecessor tiles that tile (b, q) depends on.
    """
    # nb = number of item-blocks (rows of the tile grid).
    num_item_blocks = ceil(num_items / item_block)
    # nq = number of capacity-blocks (columns of the tile grid).
    num_cap_blocks = ceil((capacity + 1) / cap_block)

    # edges[(b, q)] = set of predecessor tiles (b', q') that tile (b, q) depends on.
    edges = {}
    for b in range(num_item_blocks):
        for q in range(num_cap_blocks):
            edges[(b, q)] = set()

    for b in range(num_item_blocks):
        # Item indices inside this block (only weights are needed here).
        i_start = b * item_block
        i_end = min((b + 1) * item_block, num_items)
        for q in range(num_cap_blocks):
            dst = (b, q)
            # Always depend on the block straight above in the previous item-block.
            if b >= 1:
                edges[dst].add((b - 1, q))
            for i in range(i_start, i_end):
                wi = weights[i]
                # Capacity range that, after subtracting this item's weight, falls
                # inside the current block. This is where "take item" reads from.
                lo = q * cap_block - wi
                hi = (q + 1) * cap_block - 1 - wi
                if hi < 0 or lo > capacity:
                    continue
                lo = max(lo, 0)
                hi = min(hi, capacity)
                # Any capacity-block (in the row above) overlapping [lo, hi] is a predecessor.
                for qp in range(num_cap_blocks):
                    if qp * cap_block <= hi and (qp + 1) * cap_block - 1 >= lo:
                        edges[dst].add((b - 1, qp))
                # Blocks to the left in the SAME row also overlap [lo, hi]: predecessors.
                for qp in range(q):
                    if qp * cap_block <= hi and (qp + 1) * cap_block - 1 >= lo:
                        edges[dst].add((b, qp))
    return num_item_blocks, num_cap_blocks, edges


def main():
    """Build a random instance, verify the DAG, and check the tiled solver matches classic DP."""
    random.seed(12345)
    n, c, item_block, cap_block = parse_args()
    if cap_block is None:
        cap_block = max(1, (c + 1) // 10)

    # Random instance: n items with weight in [1,100] and value in [1,10].
    weights = [random.randint(1, 100) for _ in range(n)]
    values = [random.randint(1, 10) for _ in range(n)]

    # Reference result from the classic full DP.
    dp = build_dp(n, c, weights, values)

    # Build the dependency DAG of tiles.
    nb, nq, edges = build_graph(n, c, weights, item_block, cap_block)

    node_count = nb * nq
    edge_count = sum(len(s) for s in edges.values())
    print(f"items={n} capacity={c} item_block={item_block} cap_block={cap_block}")
    print(f"tile grid: {nb} x {nq} = {node_count} nodes, {edge_count} edges")
    print(f"avg out-degree (edges per node): {edge_count / node_count:.2f}")

    # Sanity check: the DAG must respect row-major topological order (b,q),
    # i.e. every predecessor comes strictly before the tile.
    for (b, q), preds in sorted(edges.items()):
        for src in preds:
            ok = src[0] < b or (src[0] == b and src[1] < q)
            assert ok, f"DAG violation: {src} -> {(b, q)}"
    print("DAG check OK: edges respect row-major topological order (b,q)")

    # Show the dependencies of one concrete tile for inspection.
    sample = (2, 3)
    if sample in edges:
        preds = sorted(edges[sample])
        lo = sample[1] * cap_block
        hi = (sample[1] + 1) * cap_block - 1
        above = {x for x in preds if x[1] == sample[1]}
        offset = set(preds) - above
        print(f"\nsample tile {sample} (capacity [{lo},{hi}]):")
        print(f"  dependencies (from item-block {sample[0]-1}): {preds}")
        print(f"  straight-above: {sorted(above)}")
        print(f"  weight-offset:  {sorted(offset)}")

    print(f"\nbest value dp[{n}][{c}] = {dp[n][c]}")

    # Solve with the tiled DAG and compare against the classic DP.
    tiles, stats = solve_dag_tiled(n, c, weights, values, item_block, cap_block, nb, nq)
    # Best value = last row, last column of the last (bottom-right) tile.
    dag_val = tiles[(nb - 1, nq - 1)][-1][c - (nq - 1) * cap_block]
    if dag_val == dp[n][c]:
        print(f"RESULT MATCH: dag==classic  value={dag_val}")
    else:
        print("RESULT MISMATCH between dag and classic dp!")

    # --- [CONTEggio per tile - evidenziato: togliere se non serve] -----------
    print("\nper-tile work counters:")
    total_r = total_w = 0
    for (b, q), s in sorted(stats.items()):
        total_r += s["reads"]
        total_w += s["writes"]
    print(f"  total reads:  {total_r}")
    print(f"  total writes: {total_w}")
    print(f"  classic full-grid cells: {(n + 1) * (c + 1)}")
    # -------------------------------------------------------------------------


if __name__ == "__main__":
    main()
