Complete Setup Documentation
Phase 1: System Prerequisites
bash# Install build tools
sudo apt install -y build-essential cmake git wget curl

# Install OpenMPI (was already installed on your system)
sudo apt install -y openmpi-bin libopenmpi-dev

# Install system METIS (partial, needed as base)
sudo apt install -y libmetis-dev metis

Phase 2: Clone Repositories
bashcd ~/Downloads

# Clone ParMETIS (includes GKlib as subdirectory)
git clone https://github.com/KarypisLab/ParMETIS.git

# Clone GKlib inside ParMETIS folder
cd ParMETIS
git clone https://github.com/KarypisLab/GKlib.git

# Clone METIS separately
cd ~/Downloads
git clone https://github.com/KarypisLab/METIS.git

Phase 3: Fix METIS Header
bashnano ~/Downloads/METIS/include/metis.h
Find these two commented lines:
c//#define IDXTYPEWIDTH 32
//#define REALTYPEWIDTH 32
Change to:
c#define IDXTYPEWIDTH 64
#define REALTYPEWIDTH 64
Save with Ctrl+X → Y → Enter
Why: These were commented out so METIS didn't know what data type width to use, causing idx_t and real_t to be undefined everywhere.

Phase 4: Build GKlib
bashcd ~/Downloads/ParMETIS/GKlib
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/local
make -j4
make install
Why: GKlib is the base utility library. Both METIS and ParMETIS depend on it so it must be built first.

Phase 5: Build METIS from Source
bashcd ~/Downloads/METIS
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/local \
         -DGKLIB_PATH=~/Downloads/ParMETIS/GKlib \
         -DSHARED=1
make -j4
make install
Why: Ubuntu 25.10's system METIS was too old — it was missing newer options like METIS_OPTION_NIPARTS, METIS_OPTION_DROPEDGES, METIS_OPTION_ONDISK that ParMETIS needs.

Phase 6: Build ParMETIS from Source
bashcd ~/Downloads/ParMETIS
rm -rf build
mkdir build && cd build

cmake .. -DCMAKE_INSTALL_PREFIX=~/local \
         -DGKLIB_PATH=~/Downloads/ParMETIS/GKlib \
         -DMETIS_PATH=~/Downloads/METIS \
         -DMPI_C_INCLUDE_PATH=/usr/lib/x86_64-linux-gnu/openmpi/include \
         -DMPI_C_LIBRARIES=/usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so \
         -DCMAKE_C_COMPILER=mpicc

make -j4
make install
Why the specific flags:

-DGKLIB_PATH → Points to our freshly built GKlib
-DMETIS_PATH → Points to our freshly built METIS
-DMPI_C_INCLUDE_PATH → Tells CMake exactly where mpi.h lives
-DMPI_C_LIBRARIES → Tells CMake where MPI library files are
-DCMAKE_C_COMPILER=mpicc → Forces CMake to use MPI's compiler wrapper instead of plain gcc which doesn't know about MPI


Phase 7: Add to Environment
bashecho 'export PATH=$PATH:~/local/bin' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/local/lib' >> ~/.bashrc
echo 'export CPATH=$CPATH:~/local/include' >> ~/.bashrc
source ~/.bashrc
Why: So your system can find the libraries and headers when compiling your assignment code.

Phase 8: Verify Installation
bashls ~/local/lib | grep -E "metis|parmetis"
ls ~/local/include | grep -E "metis|parmetis"
mpicc --version
```
**Expected output:**
```
libmetis.a
libparmetis.a
metis.h
parmetis.h
gcc 15.2.0

Why Each Tool Was Needed
ToolRoleAnalogyGKlibUtility library both needToolbox 🔧METISGraph partitioning algorithmEngineer 📐ParMETISParallel MPI version of METISTeam of Engineers 👥OpenMPIMessage passing between processesWalkie Talkies 📡