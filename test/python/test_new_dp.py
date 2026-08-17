import argparse
import random
from math import ceil


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("n", nargs="?", type=int, default=100, help="number of items")
    parser.add_argument("--capacity", type=int, default=10000, help="knapsack capacity")
    parser.add_argument("--item-block", type=int, default=10, help="items per tile row")
    parser.add_argument("--cap-block", type=int, default=None, help="capacities per tile column")
    args = parser.parse_args()
    return args.n, args.capacity, args.item_block, args.cap_block


def build_dp(n, c, w, p):
    dp = [[0] * (c + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        wi, pi = w[i - 1], p[i - 1]
        for wp in range(0, c + 1):
            dp[i][wp] = dp[i - 1][wp]
            if wi <= wp:
                cand = dp[i - 1][wp - wi] + pi
                if cand > dp[i][wp]:
                    dp[i][wp] = cand
    return dp


def solve_dag_tiled(n, c, w, p, item_block, cap_block, nb, nq):
    tiles = {}
    # --- [CONTEggio per tile - evidenziato: togliere se non serve] -----------
    stats = {(b, q): {"reads": 0, "writes": 0} for b in range(nb) for q in range(nq)}
    # -------------------------------------------------------------------------
    for b in range(nb):
        i_start = b * item_block
        i_end = min((b + 1) * item_block, n)
        rows_local = i_end - i_start
        block_rows = {}
        for q in range(nq):
            wq = min(cap_block, c + 1 - q * cap_block)
            block_rows[(b, q)] = [[0] * wq for _ in range(rows_local + 1)]
        if b == 0:
            for q in range(nq):
                block_rows[(b, q)][0] = [0] * len(block_rows[(b, q)][0])
        else:
            for q in range(nq):
                wq = len(block_rows[(b, q)][0])
                for c_local in range(wq):
                    wp = q * cap_block + c_local
                    q_prev = wp // cap_block
                    c_local_prev = wp - q_prev * cap_block
                    block_rows[(b, q)][0][c_local] = \
                        tiles[(b - 1, q_prev)][-1][c_local_prev]
                    # --- [CONTEggio - evidenziato] ---
                    stats[(b, q)]["reads"] += 1
                    # ----------------------------------
        for a in range(rows_local):
            i = i_start + a
            wi, pi = w[i], p[i]
            for q in range(nq):
                lo = q * cap_block
                wq = len(block_rows[(b, q)][0])
                prev = block_rows[(b, q)][a]
                nxt = block_rows[(b, q)][a + 1]
                for c_local in range(wq):
                    wp = lo + c_local
                    v = prev[c_local]
                    stats[(b, q)]["reads"] += 1
                    if wi <= wp:
                        src = wp - wi
                        if src >= lo:
                            cand = prev[src - lo] + pi
                            stats[(b, q)]["reads"] += 1
                        else:
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


def build_graph(n, c, w, item_block, cap_block):
    nb = ceil(n / item_block)
    nq = ceil((c + 1) / cap_block)

    edges = {}
    for b in range(nb):
        for q in range(nq):
            edges[(b, q)] = set()

    for b in range(nb):
        i_start = b * item_block
        i_end = min((b + 1) * item_block, n)
        for q in range(nq):
            dst = (b, q)
            if b >= 1:
                edges[dst].add((b - 1, q))
            for i in range(i_start, i_end):
                wi = w[i]
                lo = q * cap_block - wi
                hi = (q + 1) * cap_block - 1 - wi
                if hi < 0 or lo > c:
                    continue
                lo = max(lo, 0)
                hi = min(hi, c)
                for qp in range(nq):
                    if qp * cap_block <= hi and (qp + 1) * cap_block - 1 >= lo:
                        edges[dst].add((b - 1, qp))
                for qp in range(q):
                    if qp * cap_block <= hi and (qp + 1) * cap_block - 1 >= lo:
                        edges[dst].add((b, qp))
    return nb, nq, edges


def main():
    random.seed(12345)
    n, c, item_block, cap_block = parse_args()
    if cap_block is None:
        cap_block = max(1, (c + 1) // 10)

    w = [random.randint(1, 100) for _ in range(n)]
    p = [random.randint(1, 10) for _ in range(n)]

    dp = build_dp(n, c, w, p)

    nb, nq, edges = build_graph(n, c, w, item_block, cap_block)

    node_count = nb * nq
    edge_count = sum(len(s) for s in edges.values())
    print(f"items={n} capacity={c} item_block={item_block} cap_block={cap_block}")
    print(f"tile grid: {nb} x {nq} = {node_count} nodes, {edge_count} edges")
    print(f"avg out-degree (edges per node): {edge_count / node_count:.2f}")

    for (b, q), preds in sorted(edges.items()):
        for src in preds:
            ok = src[0] < b or (src[0] == b and src[1] < q)
            assert ok, f"DAG violation: {src} -> {(b, q)}"
    print("DAG check OK: edges respect row-major topological order (b,q)")

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

    tiles, stats = solve_dag_tiled(n, c, w, p, item_block, cap_block, nb, nq)
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