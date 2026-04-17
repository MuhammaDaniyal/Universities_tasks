mpicxx -O2 -o /home/mpi/Assignment3/src/pagerank_s1 \
    /home/mpi/Assignment3/src/pagerank_s1.cpp \
    -I/usr/local/include -L/usr/local/lib \
    -lparmetis -lmetis -lm

scp /home/mpi/Assignment3/src/pagerank_s1 \
    mpi@slave:/home/mpi/Assignment3/src/

mpirun -np 2 -hostfile ~/machinefile \
    /home/mpi/Assignment3/src/pagerank_s1 \
    /home/mpi/Assignment3/web-Google.metis


_________________________________________________


mpicxx -O2 -o /home/mpi/Assignment3/src/pagerank_s2 \
    /home/mpi/Assignment3/src/pagerank_s2.cpp \
    -I/usr/local/include -L/usr/local/lib \
    -lparmetis -lmetis -lm

scp /home/mpi/Assignment3/src/pagerank_s2 \
    mpi@slave:/home/mpi/Assignment3/src/

mpirun -np 2 -hostfile ~/machinefile \
    /home/mpi/Assignment3/src/pagerank_s2 \
    /home/mpi/Assignment3/web-Google.metis

____________________________________________________


mpicxx -O2 -o /home/mpi/Assignment3/src/pagerank_s3 \
    /home/mpi/Assignment3/src/pagerank_s3.cpp \
    -I/usr/local/include -L/usr/local/lib \
    -lparmetis -lmetis -lm

scp /home/mpi/Assignment3/src/pagerank_s3 \
    mpi@slave:/home/mpi/Assignment3/src/

mpirun -np 2 -hostfile ~/machinefile \
    /home/mpi/Assignment3/src/pagerank_s3 \
    /home/mpi/Assignment3/web-Google.metis