#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"

#define N 1

int main(int argc, char **argv) {
    int numprocs, rank, new_rank, namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    MPI_Status status;
    int rc, i, j, v;

    rc = MPI_Init(&argc,&argv);
    if (rc != MPI_SUCCESS) {
        printf ("Error starting MPI program. Terminating.\n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Get_processor_name(processor_name, &namelen);
    printf("Process %d on %s out of %d\n", rank, processor_name, numprocs);


    /********************** CREATE A NEW COMMUNICATION GROUP ********************/
    MPI_Group orig_group, new_group; 
    MPI_Comm new_comm; 

    // Extract the original group handle
    MPI_Comm_group(MPI_COMM_WORLD, &orig_group); 

    // add the processes to the new group
    int ranges1[1][3];
    ranges1[0][0] = 0;
    ranges1[0][1] = numprocs-1;
    ranges1[0][2] = 1;
    MPI_Group_range_incl(orig_group, 1, ranges1, &new_group);

    MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm); 
    MPI_Group_rank (new_group, &new_rank);
    printf("rank= %d newrank= %d\n",rank,new_rank);


    /********************** DO WORK ********************/
    if (rank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);

        for (i=0; i<N; i++) {
            //send a message
            v=rank;
            printf("Process %d broadcasts the %d-th message: %d\n", new_rank, i, rank);
            MPI_Bcast(&v, 1, MPI_INT, 0, new_comm);

            //wait for a reply
            for (j=1; j<numprocs; j++) {
                MPI_Recv(&v, 1, MPI_INT, j, 0, MPI_COMM_WORLD, &status);
                printf("Process %d receives the %d-th message from %d: %d\n", rank, i, j, v);
            }
        }
    } else {
        MPI_Barrier(MPI_COMM_WORLD);

        for (i=0; i<N; i++) {
            //wait for a reply
            MPI_Bcast(&v, 1, MPI_INT, 0, new_comm);
            //MPI_Recv(&v, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            printf("Process %d receives the %d-th message from %d: %d\n", rank, i, j, v);

            //send a message
            v = rank;
            printf("Process %d sends the %d-th message: %d\n", rank, i, rank);
            MPI_Send(&v, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }


    MPI_Finalize();
    return 0;
}
