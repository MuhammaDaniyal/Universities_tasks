/*
 * Name: Muhammad Daniyal
 * Roll Number: i230579
 * Section: F
 * Assignment: 3
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
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

    
    idx_t total_nodes = 0, total_edges = 0;
    vector<idx_t> full_row_start, full_neighbors;

    if (my_rank == 0)
        read_graph(graph_file, total_nodes, total_edges,
                   full_row_start, full_neighbors);

    MPI_Bcast(&total_nodes, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_edges, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    
    vector<idx_t> node_owner(num_procs + 1);
    for (int p = 0; p <= num_procs; p++)
        node_owner[p] = (idx_t)p * total_nodes / num_procs;

    idx_t my_first_node = node_owner[my_rank];
    idx_t my_node_count = node_owner[my_rank + 1] - my_first_node;

    

    
    vector<int> row_send_counts(num_procs, 0);
    vector<int> row_send_displs(num_procs, 0);
    for (int p = 0; p < num_procs; p++) {
        row_send_counts[p] = (int)(node_owner[p+1] - node_owner[p] + 1);
        row_send_displs[p] = (int)node_owner[p];
    }

    
    vector<idx_t> my_row_start(my_node_count + 1);
    MPI_Scatterv(
        (my_rank == 0) ? full_row_start.data() : nullptr,
        row_send_counts.data(),
        row_send_displs.data(),
        MPI_LONG,
        my_row_start.data(),
        (int)(my_node_count + 1),
        MPI_LONG,
        0,
        MPI_COMM_WORLD
    );

    
    vector<int> edge_send_counts(num_procs, 0);
    vector<int> edge_send_displs(num_procs, 0);
    if (my_rank == 0) {
        for (int p = 0; p < num_procs; p++) {
            edge_send_counts[p] = (int)(full_row_start[node_owner[p+1]]
                                      - full_row_start[node_owner[p]]);
            edge_send_displs[p] = (int)full_row_start[node_owner[p]];
        }
    }
    
    MPI_Bcast(edge_send_counts.data(), num_procs, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(edge_send_displs.data(), num_procs, MPI_INT, 0, MPI_COMM_WORLD);

    idx_t my_edge_count = edge_send_counts[my_rank];
    vector<idx_t> my_neighbors(my_edge_count);

    MPI_Scatterv(
        (my_rank == 0) ? full_neighbors.data() : nullptr,
        edge_send_counts.data(),
        edge_send_displs.data(),
        MPI_LONG,
        my_neighbors.data(),
        (int)my_edge_count,
        MPI_LONG,
        0,
        MPI_COMM_WORLD
    );

    
    idx_t edge_offset = my_row_start[0];
    for (idx_t i = 0; i <= my_node_count; i++)
        my_row_start[i] -= edge_offset;

    
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

    
    vector<int> out_degree(total_nodes, 0);
    if (my_rank == 0)
        for (idx_t v = 0; v < total_nodes; v++)
            out_degree[v] = (int)(full_row_start[v+1] - full_row_start[v]);

    
    const long long chunk_size = 500000;
    for (long long offset = 0; offset < (long long)total_nodes; offset += chunk_size) {
        int count = (int)min(chunk_size,
                             (long long)total_nodes - offset);
        MPI_Bcast(out_degree.data() + offset, count,
                  MPI_INT, 0, MPI_COMM_WORLD);
    }

    
    vector<int> recv_counts(num_procs);
    vector<int> recv_displs(num_procs);
    for (int p = 0; p < num_procs; p++) {
        recv_counts[p] = (int)(node_owner[p+1] - node_owner[p]);
        recv_displs[p] = (int)node_owner[p];
    }

    
    vector<double> pagerank(total_nodes, 1.0 / total_nodes);
    vector<double> pagerank_new(total_nodes, 0.0);

    double start_time = MPI_Wtime();
    int    iteration  = 0;

    for (iteration = 0; iteration < MAX_ITER; iteration++) {

        
        vector<double> contribution(total_nodes, 0.0);

        for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
            idx_t v = my_first_node + local_i;

            if (out_degree[v] == 0) continue; 

            double push_val = pagerank[v] / out_degree[v];

            for (idx_t e = my_row_start[local_i];
                       e < my_row_start[local_i + 1]; e++) {
                contribution[my_neighbors[e]] += push_val;
            }
        }

        
        vector<double> global_contrib(total_nodes, 0.0);
        for (long long offset = 0; offset < (long long)total_nodes;
             offset += chunk_size) {
            int count = (int)min(chunk_size,
                                 (long long)total_nodes - offset);
            MPI_Allreduce(contribution.data()   + offset,
                          global_contrib.data() + offset,
                          count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        }

        
        fill(pagerank_new.begin(), pagerank_new.end(), 0.0);
        for (idx_t local_i = 0; local_i < my_node_count; local_i++) {
            idx_t v = my_first_node + local_i;
            pagerank_new[v] = (1.0 - DAMPING) / total_nodes
                            + DAMPING * global_contrib[v];
        }

        
        MPI_Allgatherv(
            pagerank_new.data() + my_first_node,  
            (int)my_node_count,                   
            MPI_DOUBLE,
            pagerank_new.data(),                  
            recv_counts.data(),                   
            recv_displs.data(),                   
            MPI_DOUBLE,
            MPI_COMM_WORLD
        );

        
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

    
    if (my_rank == 0) {
        cout << "\n===== Scenario 2 (Collective - Allgatherv) =====" << endl;
        cout << "Processes  : " << num_procs  << endl;
        cout << "Iterations : " << iteration  << endl;
        cout << "Time       : " << elapsed    << " seconds" << endl;

        
        cout << "\nTop 5 PageRank scores:" << endl;
        vector<pair<double,idx_t>> scores;
        for (idx_t v = 0; v < total_nodes; v++)
            scores.push_back({pagerank[v], v});
        sort(scores.rbegin(), scores.rend());
        for (int i = 0; i < 5; i++)
            cout << "  Node " << scores[i].second
                 << "  PR=" << scores[i].first << endl;

        
        ofstream results("/home/mpi/Assignment3/results/s2_timing.txt",
                         ios::app);
        if (results.is_open())
            results << "procs=" << num_procs
                    << "  iters=" << iteration
                    << "  time="  << elapsed  << "\n";
    }

    MPI_Finalize();
    return 0;
}