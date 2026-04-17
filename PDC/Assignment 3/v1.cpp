cat > /home/mpi/Assignment3/src/pagerank_s1.cpp << 'EOF'
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <mpi.h>

using namespace std;

const double DAMPING   = 0.85;
const double THRESHOLD = 1e-7;
const int    MAX_ITER  = 200;

void read_graph(const string      &filename,
                long long         &total_nodes,
                long long         &total_edges,
                vector<long long> &row_start,
                vector<long long> &neighbors)
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
    for (long long node = 0; node < total_nodes; node++) {
        row_start[node] = (long long)neighbors.size();
        if (!getline(file, line)) continue;
        istringstream ls(line);
        long long nb;
        while (ls >> nb)
            neighbors.push_back(nb - 1); 
    }
    row_start[total_nodes] = (long long)neighbors.size();
    total_edges = (long long)neighbors.size();
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int my_rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    string graph_file = (argc > 1)
                      ? argv[1]
                      : "/home/mpi/Assignment3/web-Google.metis";

    long long total_nodes = 0, total_edges = 0;
    vector<long long> full_row_start, full_neighbors;

    if (my_rank == 0)
        read_graph(graph_file, total_nodes, total_edges,
                   full_row_start, full_neighbors);

    MPI_Bcast(&total_nodes, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_edges, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    vector<long long> node_owner(num_procs + 1);
    for (int p = 0; p <= num_procs; p++)
        node_owner[p] = (long long)p * total_nodes / num_procs;

    long long my_first_node = node_owner[my_rank];
    long long my_node_count = node_owner[my_rank + 1] - my_first_node;

    vector<long long> my_row_start(my_node_count + 1);
    if (my_rank == 0) {
        copy(full_row_start.begin(),
             full_row_start.begin() + my_node_count + 1,
             my_row_start.begin());
        for (int p = 1; p < num_procs; p++) {
            long long p_size = node_owner[p+1] - node_owner[p] + 1;
            MPI_Send(&p_size, 1, MPI_LONG_LONG, p, 0, MPI_COMM_WORLD);
            MPI_Send(full_row_start.data() + node_owner[p],
                     (int)p_size, MPI_LONG_LONG, p, 1, MPI_COMM_WORLD);
        }
    } else {
        long long recv_size;
        MPI_Recv(&recv_size, 1, MPI_LONG_LONG,
                 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(my_row_start.data(), (int)recv_size, MPI_LONG_LONG,
                 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    long long my_edge_count = my_row_start[my_node_count] - my_row_start[0];
    vector<long long> my_neighbors(my_edge_count);
    if (my_rank == 0) {
        copy(full_neighbors.begin(),
             full_neighbors.begin() + my_edge_count,
             my_neighbors.begin());
        for (int p = 1; p < num_procs; p++) {
            long long p_edge_start = full_row_start[node_owner[p]];
            long long p_edge_end   = full_row_start[node_owner[p+1]];
            long long p_edge_count = p_edge_end - p_edge_start;
            MPI_Send(&p_edge_count, 1, MPI_LONG_LONG, p, 2, MPI_COMM_WORLD);
            MPI_Send(full_neighbors.data() + p_edge_start,
                     (int)p_edge_count, MPI_LONG_LONG, p, 3, MPI_COMM_WORLD);
        }
    } else {
        long long recv_size;
        MPI_Recv(&recv_size, 1, MPI_LONG_LONG,
                 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        my_neighbors.resize(recv_size);
        MPI_Recv(my_neighbors.data(), (int)recv_size, MPI_LONG_LONG,
                 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    long long edge_offset = my_row_start[0];
    for (long long i = 0; i <= my_node_count; i++)
        my_row_start[i] -= edge_offset;

    vector<int> out_degree(total_nodes, 0);
    if (my_rank == 0)
        for (long long v = 0; v < total_nodes; v++)
            out_degree[v] = (int)(full_row_start[v+1] - full_row_start[v]);

    long long chunk_size = 10000000;
    for (long long offset = 0; offset < total_nodes; offset += chunk_size) {
        int count = (int)min(chunk_size, total_nodes - offset);
        MPI_Bcast(out_degree.data() + offset, count, MPI_INT, 0, MPI_COMM_WORLD);
    }

    vector<double> pagerank(total_nodes, 1.0 / total_nodes);

    double start_time = MPI_Wtime();
    int    iteration  = 0;

    for (iteration = 0; iteration < MAX_ITER; iteration++) {

        vector<double> contribution(total_nodes, 0.0);

        for (long long local_i = 0; local_i < my_node_count; local_i++) {
            long long v = my_first_node + local_i;

            if (out_degree[v] == 0) continue;

            double push_value = pagerank[v] / out_degree[v];

            for (long long e = my_row_start[local_i];
                           e < my_row_start[local_i + 1]; e++) {
                long long u = my_neighbors[e];
                contribution[u] += push_value;
            }
        }

        vector<double> global_contribution(total_nodes, 0.0);
        for (long long offset = 0; offset < total_nodes; offset += chunk_size) {
            int count = (int)min(chunk_size, total_nodes - offset);
            MPI_Allreduce(contribution.data() + offset,
                          global_contribution.data() + offset,
                          count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        }

        vector<double> pagerank_new(total_nodes, 0.0);
        for (long long local_i = 0; local_i < my_node_count; local_i++) {
            long long v = my_first_node + local_i;
            pagerank_new[v] = (1.0 - DAMPING) / total_nodes
                            + DAMPING * global_contribution[v];
        }

        if (my_rank == 0) {
            for (int p = 1; p < num_procs; p++) {
                long long p_first = node_owner[p];
                long long p_count = node_owner[p+1] - p_first;

                long long incoming_size;
                MPI_Recv(&incoming_size, 1, MPI_LONG_LONG,
                         p, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                MPI_Recv(pagerank_new.data() + p_first,
                         (int)incoming_size, MPI_DOUBLE,
                         p, 11, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else {
            MPI_Send(&my_node_count, 1, MPI_LONG_LONG,
                     0, 10, MPI_COMM_WORLD);
            MPI_Send(pagerank_new.data() + my_first_node,
                     (int)my_node_count, MPI_DOUBLE,
                     0, 11, MPI_COMM_WORLD);
        }

        for (long long offset = 0; offset < total_nodes; offset += chunk_size) {
            int count = (int)min(chunk_size, total_nodes - offset);
            MPI_Bcast(pagerank_new.data() + offset, count,
                      MPI_DOUBLE, 0, MPI_COMM_WORLD);
        }

        double my_change    = 0.0;
        double total_change = 0.0;
        for (long long v = my_first_node;
                       v < my_first_node + my_node_count; v++)
            my_change += fabs(pagerank_new[v] - pagerank[v]);

        MPI_Allreduce(&my_change, &total_change, 1,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        swap(pagerank, pagerank_new);

        if (total_change < THRESHOLD) { iteration++; break; }
    }

    double elapsed = MPI_Wtime() - start_time;

    if (my_rank == 0) {
        cout << "\n===== Scenario 1 (P2P Blocking) =====" << endl;
        cout << "Processes  : " << num_procs << endl;
        cout << "Iterations : " << iteration << endl;
        cout << "Time       : " << elapsed   << " seconds" << endl;

        ofstream results("/home/mpi/Assignment3/results/s1_timing.txt",
                         ios::app);
        if (results.is_open())
            results << "procs=" << num_procs
                    << "  iters=" << iteration
                    << "  time=" << elapsed << "\n";
    }

    MPI_Finalize();
    return 0;
}
EOF