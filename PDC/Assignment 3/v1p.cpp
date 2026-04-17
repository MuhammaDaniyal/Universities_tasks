cat > /home/mpi/Assignment3/src/pagerank_s1.cpp << 'EOF'
/*
 * ============================================================
 * Scenario 1: Distributed PageRank - P2P Blocking
 * Course: Parallel and Distributed Computing (CS-3006)
 *
 * Complete flow:
 *   1. Rank 0 reads graph
 *   2. Even split distribution (for ParMETIS input)
 *   3. ParMETIS partition (optimizes distribution)
 *   4. REDISTRIBUTE based on partition
 *   5. PageRank iterations with P2P blocking
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>
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

    /* ========== PHASE 1: Read Graph ========== */
    idx_t total_nodes = 0, total_edges = 0;
    vector<idx_t> full_row_start, full_neighbors;

    if (my_rank == 0)
        read_graph(graph_file, total_nodes, total_edges,
                   full_row_start, full_neighbors);

    MPI_Bcast(&total_nodes, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_edges, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    /* ========== PHASE 2: Even Split (Initial Distribution) ========== */
    vector<idx_t> node_owner(num_procs + 1);
    for (int p = 0; p <= num_procs; p++)
        node_owner[p] = (idx_t)p * total_nodes / num_procs;

    idx_t my_first_node = node_owner[my_rank];
    idx_t my_node_count = node_owner[my_rank + 1] - my_first_node;

    /* Distribute graph chunks (even split) */
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
        MPI_Recv(&recv_size, 1, MPI_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
        MPI_Recv(&recv_size, 1, MPI_LONG, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        my_neighbors.resize(recv_size);
        MPI_Recv(my_neighbors.data(), (int)recv_size, MPI_LONG,
                 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    idx_t edge_offset = my_row_start[0];
    for (idx_t i = 0; i <= my_node_count; i++)
        my_row_start[i] -= edge_offset;

    /* ========== PHASE 3: ParMETIS Partitioning ========== */
    idx_t  weight_flag  = 0;
    idx_t  index_flag   = 0;
    idx_t  num_weights  = 1;
    idx_t  num_parts    = num_procs;
    real_t imbalance    = 1.05f;
    idx_t  edge_cut     = 0;
    idx_t  options[3]   = {0, 0, 0};

    vector<real_t> target_weights(num_procs, 1.0f / num_procs);
    vector<idx_t>  partition(my_node_count);

    MPI_Comm comm = MPI_COMM_WORLD;
    ParMETIS_V3_PartKway(
        node_owner.data(),
        my_row_start.data(),
        my_neighbors.data(),
        NULL, NULL,
        &weight_flag, &index_flag,
        &num_weights, &num_parts,
        target_weights.data(),
        &imbalance, options,
        &edge_cut, partition.data(), &comm
    );

    if (my_rank == 0)
        cout << "ParMETIS edge cut: " << edge_cut << endl;

    /* ========== PHASE 4: Redistribute Based on ParMETIS ========== */
    // Build send lists: which nodes go to which process
    vector<vector<idx_t>> send_to(num_procs);
    for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
        int target = partition[local_i];
        if (target != my_rank) {
            send_to[target].push_back(my_first_node + local_i);
        }
    }

    // Exchange node IDs
    vector<int> send_counts(num_procs);
    vector<int> recv_counts(num_procs);
    for (int p = 0; p < num_procs; p++)
        send_counts[p] = send_to[p].size();

    MPI_Alltoall(send_counts.data(), 1, MPI_INT,
                 recv_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    // Receive buffers
    vector<vector<idx_t>> recv_from(num_procs);
    vector<MPI_Request> requests;
    
    for (int p = 0; p < num_procs; p++) {
        if (p == my_rank) continue;
        
        if (send_counts[p] > 0) {
            MPI_Request req;
            MPI_Isend(send_to[p].data(), send_counts[p], MPI_LONG,
                      p, 100, MPI_COMM_WORLD, &req);
            requests.push_back(req);
        }
        
        if (recv_counts[p] > 0) {
            recv_from[p].resize(recv_counts[p]);
            MPI_Request req;
            MPI_Irecv(recv_from[p].data(), recv_counts[p], MPI_LONG,
                      p, 100, MPI_COMM_WORLD, &req);
            requests.push_back(req);
        }
    }
    
    MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
    
    // Build new global node list
    vector<idx_t> my_new_global_nodes;
    
    // Keep nodes that should stay
    for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
        if (partition[local_i] == my_rank) {
            my_new_global_nodes.push_back(my_first_node + local_i);
        }
    }
    
    // Add received nodes
    for (int p = 0; p < num_procs; p++) {
        for (idx_t node : recv_from[p]) {
            my_new_global_nodes.push_back(node);
        }
    }
    
    sort(my_new_global_nodes.begin(), my_new_global_nodes.end());
    
    // Build new node ownership array
    vector<idx_t> final_node_ownership(num_procs + 1, 0);
    vector<int> all_counts(num_procs);
    int my_count = my_new_global_nodes.size();
    MPI_Allgather(&my_count, 1, MPI_INT,
                  all_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    
    for (int p = 0; p < num_procs; p++)
        final_node_ownership[p+1] = final_node_ownership[p] + all_counts[p];
    
    // Rebuild local CSR using broadcasted full graph
    vector<idx_t> full_row_start_local(total_nodes + 1);
    vector<idx_t> full_neighbors_local(total_edges);
    
    if (my_rank == 0) {
        full_row_start_local = full_row_start;
        full_neighbors_local = full_neighbors;
    }
    MPI_Bcast(full_row_start_local.data(), total_nodes + 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(full_neighbors_local.data(), total_edges, MPI_LONG, 0, MPI_COMM_WORLD);
    
    // Build new local CSR
    idx_t new_node_count = my_new_global_nodes.size();
    vector<idx_t> new_row_start(new_node_count + 1);
    vector<idx_t> new_neighbors;
    
    new_row_start[0] = 0;
    for (idx_t local_i = 0; local_i < new_node_count; local_i++) {
        idx_t global_node = my_new_global_nodes[local_i];
        idx_t edge_begin = full_row_start_local[global_node];
        idx_t edge_end   = full_row_start_local[global_node + 1];
        
        for (idx_t e = edge_begin; e < edge_end; e++)
            new_neighbors.push_back(full_neighbors_local[e]);
        
        new_row_start[local_i + 1] = new_neighbors.size();
    }
    
    // Update variables
    my_node_count = new_node_count;
    my_first_node = final_node_ownership[my_rank];
    my_row_start = new_row_start;
    my_neighbors = new_neighbors;
    
    /* ========== PHASE 5: Out-degree ========== */
    vector<int> out_degree(total_nodes, 0);
    if (my_rank == 0)
        for (idx_t v = 0; v < total_nodes; v++)
            out_degree[v] = (int)(full_row_start_local[v+1] - full_row_start_local[v]);
    MPI_Bcast(out_degree.data(), (int)total_nodes, MPI_INT, 0, MPI_COMM_WORLD);
    
    /* ========== PHASE 6: PageRank with P2P Blocking ========== */
    vector<double> pagerank    (total_nodes, 1.0 / total_nodes);
    vector<double> pagerank_new(total_nodes, 0.0);
    
    double start_time = MPI_Wtime();
    int iteration = 0;
    
    for (iteration = 0; iteration < MAX_ITER; iteration++) {
        
        // Push phase
        vector<double> contribution(total_nodes, 0.0);
        for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
            idx_t v = my_first_node + local_i;
            if (out_degree[v] == 0) continue;
            
            double push_value = pagerank[v] / out_degree[v];
            for (idx_t e = my_row_start[local_i];
                       e < my_row_start[local_i + 1]; e++) {
                contribution[my_neighbors[e]] += push_value;
            }
        }
        
        // Allreduce contributions
        vector<double> global_contribution(total_nodes, 0.0);
        MPI_Allreduce(contribution.data(), global_contribution.data(),
                      (int)total_nodes, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        
        // Compute new PageRank for my nodes
        for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
            idx_t v = my_first_node + local_i;
            pagerank_new[v] = (1.0 - DAMPING) / total_nodes
                            + DAMPING * global_contribution[v];
        }
        
        // P2P Blocking: Send to rank 0, then broadcast
        if (my_rank == 0) {
            for (int p = 1; p < num_procs; p++) {
                idx_t p_first = final_node_ownership[p];
                idx_t p_count = final_node_ownership[p+1] - p_first;
                MPI_Recv(pagerank_new.data() + p_first,
                         (int)p_count, MPI_DOUBLE,
                         p, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else {
            MPI_Send(pagerank_new.data() + my_first_node,
                     (int)my_node_count, MPI_DOUBLE,
                     0, 100, MPI_COMM_WORLD);
        }
        
        MPI_Bcast(pagerank_new.data(), (int)total_nodes,
                  MPI_DOUBLE, 0, MPI_COMM_WORLD);
        
        // Convergence check
        double my_change = 0.0;
        double total_change = 0.0;
        for (idx_t v = my_first_node; v < my_first_node + my_node_count; v++)
            my_change += fabs(pagerank_new[v] - pagerank[v]);
        
        MPI_Allreduce(&my_change, &total_change, 1,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        
        swap(pagerank, pagerank_new);
        
        if (total_change < THRESHOLD) { iteration++; break; }
    }
    
    double elapsed = MPI_Wtime() - start_time;
    
    if (my_rank == 0) {
        cout << "\n===== Scenario 1 (P2P Blocking with ParMETIS) =====" << endl;
        cout << "Processes      : " << num_procs << endl;
        cout << "Edge cut       : " << edge_cut << endl;
        cout << "Iterations     : " << iteration << endl;
        cout << "Time           : " << elapsed << " seconds" << endl;
        
        ofstream results("/home/mpi/Assignment3/results/s1_timing.txt", ios::app);
        if (results.is_open())
            results << "procs=" << num_procs
                    << "  edgecut=" << edge_cut
                    << "  iters=" << iteration
                    << "  time=" << elapsed << "\n";
    }
    
    MPI_Finalize();
    return 0;
}
EOF