import sys
from collections import defaultdict

input_file = "web-Google.txt"
output_file = "web-Google.metis"

edges = defaultdict(set)
nodes = set()

print("Reading edge list...")
with open(input_file) as f:
    for line in f:
        if line.startswith('#'):
            continue
        u, v = map(int, line.split())
        edges[u].add(v)
        edges[v].add(u)  # make undirected for METIS
        nodes.add(u)
        nodes.add(v)

# Remap node IDs to be contiguous starting from 1
node_list = sorted(nodes)
node_map = {n: i+1 for i, n in enumerate(node_list)}
N = len(node_list)
E = sum(len(v) for v in edges.values()) // 2

print(f"Nodes: {N}, Edges: {E}")

print("Writing METIS format...")
with open(output_file, 'w') as f:
    f.write(f"{N} {E}\n")
    for node in node_list:
        neighbors = [str(node_map[nb]) for nb in sorted(edges[node])]
        f.write(" ".join(neighbors) + "\n")

print(f"Done! Written to {output_file}")

