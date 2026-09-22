#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    MPI::Init(argc, argv);
    if (!MPI::Is_initialized()) {
        printf("MPI did not initialize!\n");
        return 1;
    }
    MPI::Finalize();
}
