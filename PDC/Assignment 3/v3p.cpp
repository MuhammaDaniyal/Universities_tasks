cat > /home/mpi/Assignment3/src/pagerank_s3.cpp << 'EOF'
/*
 * ============================================================
 * Scenario 3: Distributed PageRank - Async Communication Overlap
 * Course: Parallel and Distributed Computing (CS-3006)
 * Name: [YOUR NAME]
 * Roll: [YOUR ROLL NUMBER]
 * Section: [YOUR SECTION]
 *
 * CORRECT PageRank uses PUSH/SCATTER method:
 *   METIS stores OUTGOING edges (v → u)
 *   For each node v I own:
 *     For each outgoing edge v → u:
 *       contribution[u] += PR(v) / out_degree(v)
 *   Then: PR_new(u) = (1-d)/N + d * contribution[u]
 *
 * Communication: Async Overlap using MPI_Iallreduce
 *   1. Classify nodes as Internal or Boundary
 *   2. Start MPI_Iallreduce (non-blocking reduce)
 *   3. While reduce happens in background:
 *      compute Internal node PR (no remote data needed)
 *      <- THIS IS THE OVERLAP
 *   4. MPI_Waitall - reduce finishes
 *   5. Compute Boundary node PR using global contributions
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <utility>
#include <mpi.h>
#include <parmetis.h>

using namespace std;

const double DAMPING   = 0.85;
const double THRESHOLD = 1e-7;
const int    MAX_ITER  = 200;

void read_graph(const string  &filename,
                idx_t         &total_nodes,
                idx_t         &total_edges,
                vector<idx_t> &row_start,
                vector<idx_t> &neighbors)
{
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "ERROR: Cannot open " << filename << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string header;
    getline(file, header);
    istringstream hs(header);
    hs >> total_nodes >> total_edges;
    cout << "Graph: " << total_nodes << " nodes, "
                      << total_edges << " edges" << endl;

    row_start.resize(total_nodes + 1, 0);
    neighbors.reserve(total_edges);
    string line;
    for (idx_t node = 0; node < total_nodes; node++) {
        row_start[node] = (idx_t)neighbors.size();
        if (!getline(file, line)) continue;
        istringstream ls(line);
        idx_t nb;
        while (ls >> nb)
            neighbors.push_back(nb - 1);
    }
    row_start[total_nodes] = (idx_t)neighbors.size();
    total_edges = (idx_t)neighbors.size();
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int my_rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    string graph_file = (argc > 1)
                      ? argv[1]
                      : "/home/mpi/Assignment3/web-Google.metis";

    /* ────────────────────────────────────────────────────────
       STEP 1: Rank 0 reads graph, broadcasts sizes
       ──────────────────────────────────────────────────────── */
    idx_t total_nodes = 0, total_edges = 0;
    vector<idx_t> full_row_start, full_neighbors;

    if (my_rank == 0)
        read_graph(graph_file, total_nodes, total_edges,
                   full_row_start, full_neighbors);

    MPI_Bcast(&total_nodes, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_edges, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    /* ────────────────────────────────────────────────────────
       STEP 2: Even split - who owns which nodes
       ──────────────────────────────────────────────────────── */
    vector<idx_t> node_owner(num_procs + 1);
    for (int p = 0; p <= num_procs; p++)
        node_owner[p] = (idx_t)p * total_nodes / num_procs;

    idx_t my_first_node = node_owner[my_rank];
    idx_t my_node_count = node_owner[my_rank + 1] - my_first_node;

    /* ────────────────────────────────────────────────────────
       STEP 3: Distribute graph chunks via P2P blocking
               Send size first then data
       ──────────────────────────────────────────────────────── */
    vector<idx_t> my_row_start(my_node_count + 1);
    if (my_rank == 0) {
        copy(full_row_start.begin(),
             full_row_start.begin() + my_node_count + 1,
             my_row_start.begin());
        for (int p = 1; p < num_procs; p++) {
            idx_t p_size = node_owner[p+1] - node_owner[p] + 1;
            MPI_Send(&p_size, 1, MPI_LONG, p, 0, MPI_COMM_WORLD);
            MPI_Send(full_row_start.data() + node_owner[p],
                     (int)p_size, MPI_LONG, p, 1, MPI_COMM_WORLD);
        }
    } else {
        idx_t recv_size;
        MPI_Recv(&recv_size, 1, MPI_LONG,
                 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(my_row_start.data(), (int)recv_size, MPI_LONG,
                 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    idx_t my_edge_count = my_row_start[my_node_count] - my_row_start[0];
    vector<idx_t> my_neighbors(my_edge_count);

    if (my_rank == 0) {
        copy(full_neighbors.begin(),
             full_neighbors.begin() + my_edge_count,
             my_neighbors.begin());
        for (int p = 1; p < num_procs; p++) {
            idx_t p_edge_start = full_row_start[node_owner[p]];
            idx_t p_edge_end   = full_row_start[node_owner[p+1]];
            idx_t p_edge_count = p_edge_end - p_edge_start;
            MPI_Send(&p_edge_count, 1, MPI_LONG, p, 2, MPI_COMM_WORLD);
            MPI_Send(full_neighbors.data() + p_edge_start,
                     (int)p_edge_count, MPI_LONG, p, 3, MPI_COMM_WORLD);
        }
    } else {
        idx_t recv_size;
        MPI_Recv(&recv_size, 1, MPI_LONG,
                 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        my_neighbors.resize(recv_size);
        MPI_Recv(my_neighbors.data(), (int)recv_size, MPI_LONG,
                 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    idx_t edge_offset = my_row_start[0];
    for (idx_t i = 0; i <= my_node_count; i++)
        my_row_start[i] -= edge_offset;

    /* ────────────────────────────────────────────────────────
       STEP 4: ParMETIS - graph partitioning
       ──────────────────────────────────────────────────────── */
    idx_t  weight_flag = 0;
    idx_t  index_flag  = 0;
    idx_t  num_weights = 1;
    idx_t  num_parts   = num_procs;
    real_t imbalance   = 1.05f;
    idx_t  edge_cut    = 0;
    idx_t  options[3]  = {0, 0, 0};

    vector<real_t> target_weights(num_procs, 1.0f / num_procs);
    vector<idx_t>  partition(my_node_count);
    MPI_Comm comm = MPI_COMM_WORLD;

    ParMETIS_V3_PartKway(
        node_owner.data(), my_row_start.data(), my_neighbors.data(),
        NULL, NULL, &weight_flag, &index_flag, &num_weights, &num_parts,
        target_weights.data(), &imbalance, options,
        &edge_cut, partition.data(), &comm
    );

    if (my_rank == 0)
        cout << "ParMETIS edge cut: " << edge_cut << endl;

    /* ────────────────────────────────────────────────────────
       STEP 5: Out-degree broadcast
       ──────────────────────────────────────────────────────── */
    vector<int> out_degree(total_nodes, 0);
    if (my_rank == 0)
        for (idx_t v = 0; v < total_nodes; v++)
            out_degree[v] = (int)(full_row_start[v+1] - full_row_start[v]);

    const long long chunk_size = 500000;
    for (long long offset = 0; offset < (long long)total_nodes;
         offset += chunk_size) {
        int count = (int)min(chunk_size, (long long)total_nodes - offset);
        MPI_Bcast(out_degree.data() + offset, count, MPI_INT, 0, MPI_COMM_WORLD);
    }

    /* ────────────────────────────────────────────────────────
       STEP 6: Classify Internal vs Boundary nodes

       Internal: all outgoing edges stay within my range
                 can compute PR without waiting for global data
       Boundary: has edges going outside my range
                 must wait for MPI_Iallreduce to complete
       ──────────────────────────────────────────────────────── */
    vector<idx_t> internal_nodes;
    vector<idx_t> boundary_nodes;

    for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
        bool is_boundary = false;
        for (idx_t e = my_row_start[local_i];
                   e < my_row_start[local_i + 1]; e++) {
            idx_t target = my_neighbors[e];
            if (target < my_first_node ||
                target >= my_first_node + my_node_count) {
                is_boundary = true;
                break;
            }
        }
        if (is_boundary)
            boundary_nodes.push_back(local_i);
        else
            internal_nodes.push_back(local_i);
    }

    if (my_rank == 0)
        cout << "Internal: " << internal_nodes.size()
             << "  Boundary: " << boundary_nodes.size() << endl;

    /* ────────────────────────────────────────────────────────
       STEP 7: PageRank Power Iteration with Async Overlap
       ──────────────────────────────────────────────────────── */
    vector<double> pagerank(total_nodes, 1.0 / total_nodes);

    double total_comm_time    = 0.0;
    double total_overlap_time = 0.0;

    double start_time = MPI_Wtime();
    int    iteration  = 0;

    for (iteration = 0; iteration < MAX_ITER; iteration++) {

        /* Compute local contributions via PUSH method */
        vector<double> local_contribution(total_nodes, 0.0);
        for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
            idx_t v = my_first_node + local_i;
            if (out_degree[v] == 0) continue;

            double push_val = pagerank[v] / out_degree[v];
            for (idx_t e = my_row_start[local_i];
                       e < my_row_start[local_i + 1]; e++) {
                local_contribution[my_neighbors[e]] += push_val;
            }
        }

        /* Start MPI_Iallreduce - non-blocking global sum */
        vector<double> global_contribution(total_nodes, 0.0);
        vector<MPI_Request> requests;

        double comm_start = MPI_Wtime();

        for (long long offset = 0; offset < (long long)total_nodes;
             offset += chunk_size) {
            int count = (int)min(chunk_size,
                                 (long long)total_nodes - offset);
            MPI_Request req;
            MPI_Iallreduce(
                local_contribution.data()  + offset,
                global_contribution.data() + offset,
                count, MPI_DOUBLE, MPI_SUM,
                MPI_COMM_WORLD, &req
            );
            requests.push_back(req);
        }

        /* Overlap: compute Internal nodes while Iallreduce runs */
        double overlap_start = MPI_Wtime();
        vector<double> pagerank_new(total_nodes, 0.0);

        for (idx_t local_i : internal_nodes) {
            idx_t v = my_first_node + local_i;
            pagerank_new[v] = (1.0 - DAMPING) / total_nodes
                            + DAMPING * local_contribution[v];
        }

        total_overlap_time += MPI_Wtime() - overlap_start;

        /* Wait for Iallreduce to complete */
        MPI_Waitall((int)requests.size(),
                    requests.data(), MPI_STATUSES_IGNORE);
        total_comm_time += MPI_Wtime() - comm_start;

        /* Compute Boundary nodes using global contributions */
        for (idx_t local_i : boundary_nodes) {
            idx_t v = my_first_node + local_i;
            pagerank_new[v] = (1.0 - DAMPING) / total_nodes
                            + DAMPING * global_contribution[v];
        }

        /* Allgatherv to sync full PR vector */
        vector<int> recv_counts(num_procs);
        vector<int> recv_displs(num_procs);
        for (int p = 0; p < num_procs; p++) {
            recv_counts[p] = (int)(node_owner[p+1] - node_owner[p]);
            recv_displs[p] = (int)node_owner[p];
        }
        MPI_Allgatherv(
            pagerank_new.data() + my_first_node,
            (int)my_node_count, MPI_DOUBLE,
            pagerank_new.data(),
            recv_counts.data(), recv_displs.data(),
            MPI_DOUBLE, MPI_COMM_WORLD
        );

        /* L1 norm convergence check */
        double my_change    = 0.0;
        double total_change = 0.0;
        for (idx_t v = my_first_node;
                   v < my_first_node + my_node_count; v++)
            my_change += fabs(pagerank_new[v] - pagerank[v]);

        MPI_Allreduce(&my_change, &total_change, 1,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        swap(pagerank, pagerank_new);

        if (my_rank == 0)
            cout << "Iter " << iteration
                 << "  change=" << total_change << endl;

        if (total_change < THRESHOLD) { iteration++; break; }
    }

    double elapsed = MPI_Wtime() - start_time;

    /* ────────────────────────────────────────────────────────
       STEP 8: Results and overlap efficiency analysis
       ──────────────────────────────────────────────────────── */
    if (my_rank == 0) {
        double overlap_pct = 0.0;
        if (total_comm_time > 0)
            overlap_pct = min(100.0,
                (total_overlap_time / total_comm_time) * 100.0);

        cout << "\n===== Scenario 3 (Async Overlap) =====" << endl;
        cout << "Processes         : " << num_procs             << endl;
        cout << "Iterations        : " << iteration             << endl;
        cout << "Total Time        : " << elapsed               << " sec" << endl;
        cout << "Comm Time         : " << total_comm_time       << " sec" << endl;
        cout << "Overlap Time      : " << total_overlap_time    << " sec" << endl;
        cout << "Overlap Efficiency: " << overlap_pct           << "%" << endl;
        cout << "Internal nodes    : " << internal_nodes.size() << endl;
        cout << "Boundary nodes    : " << boundary_nodes.size() << endl;

        /* Top 5 ranked pages */
        cout << "\nTop 5 PageRank scores:" << endl;
        vector<pair<double,idx_t>> scores;
        for (idx_t v = 0; v < total_nodes; v++)
            scores.push_back({pagerank[v], v});
        sort(scores.rbegin(), scores.rend());
        for (int i = 0; i < 5; i++)
            cout << "  Node " << scores[i].second
                 << "  PR=" << scores[i].first << endl;

        ofstream results("/home/mpi/Assignment3/results/s3_timing.txt",
                         ios::app);
        if (results.is_open())
            results << "procs="   << num_procs
                    << "  iters=" << iteration
                    << "  time="  << elapsed
                    << "  comm="  << total_comm_time
                    << "  overlap=" << total_overlap_time
                    << "  pct="   << overlap_pct << "%\n";
    }

    MPI_Finalize();
    return 0;
}
EOF