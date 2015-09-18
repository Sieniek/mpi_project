/*****************************
Projekt z Przetwarzania Rozproszonego:
	>>Menelatorium<<
Piotr Sienkiewicz 109692
Lukasz Krawczyk 109***
06.2015
*****************************/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#define HOBO_INDEX 0
#define NURSE_INDEX 1
#define ACCESS_GRANTED 0
#define ACCESS_DENIED 1
#define ACCESS_REQUEST 9

int* parseInputValues(int argc, char** argv) {
	static int returned_values[2];
	int hobos_count, nurse_count;
	if (argc > 1) {
		hobos_count = strtol(argv[1], (char **)NULL, 10);
		nurse_count = strtol(argv[2], (char **)NULL, 10);
		printf("Hobos: %d, Nurses: %d\n", hobos_count, nurse_count);
	} else {
		printf("No parameters set! Default parameters will be used: hobos=100 and nurse=10\n");
		hobos_count = 100;
		nurse_count = 10;
	}
	returned_values[0] = hobos_count;
	returned_values[1] = nurse_count;
	return returned_values;
}

/*Set process to different roles*/
void createRole(int rank, int* roles_count) {


	MPI_Comm hobo_comm, nurse_comm;
	MPI_Comm_split(MPI_COMM_WORLD, rank < roles_count[HOBO_INDEX], 0, &hobo_comm);
	MPI_Comm_split(MPI_COMM_WORLD, rank <  roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX], 0, &nurse_comm);


	if (rank < roles_count[HOBO_INDEX]) {
		/*Hobo*/
		printf("I will be hobo in future! :D %d \n", roles_count[HOBO_INDEX]);
		hobo_live(rank, roles_count[HOBO_INDEX], hobo_comm);
	} else if (rank < roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX]) {
		/*Nurse*/
		printf("I will be nurse in future. :| \n");
		nurse_live();
	} else {
		/*Useless processes*/
		printf("I have no future... ;( \n");
	}
}

int update_lamport(int local_time, int message_time){
	return local_time > message_time ? local_time : message_time;
}

/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count, MPI_Comm my_comm) {

	int lamport;
	int message[2] = {ACCESS_REQUEST, lamport};
	int get_message[2];
	int recv_buff[hobos_count], index;

	/*Allgather will send request to all and gather others requests*/
	MPI_Allgather(&message, 1, MPI_INT, recv_buff, 1, MPI_INT, my_comm);

	/*Send verdict to all accepted and rejected hobos
	Message is different for hobos, so we can't use Bcast since it use one message,
	using Scatter/Gather/Alltoall will be not optimal since other hobos aren't intrested
	of status rest of hobos*/

	/*I can remove some communicatio here: it's because if A hobo will have to send ACCESS_GRANTED status
	to B process and B process will be not waiting for responses from C,D,... processes since we are assuming
	this responses will be tha same, only A process must to send response to B, then B can send status to C and so on,
	n-process will send status to A*/

	index = rank + 1;
	message[0] = index < 5 ? ACCESS_GRANTED : ACCESS_DENIED;

	if (index == hobos_count)
		index = 0;
	/*For 0 process it will be seperate implementation since it will connect all in "chain"*/
	if (rank == 0) {
		MPI_Send(&message, 2, MPI_INT, index, 0, my_comm);
		MPI_Recv(&get_message, 2, MPI_INT, hobos_count - 1, 0, my_comm, MPI_STATUS_IGNORE);
	} else {
		MPI_Recv(&get_message, 2, MPI_INT, rank - 1, 0, my_comm, MPI_STATUS_IGNORE);
		MPI_Send(&message, 2, MPI_INT, index, 0, my_comm);
	}
	printf("Process no. \t %d Status: \t %d\n", rank, get_message[0]);
}

/*This function will be executed by Nurses processes*/
int nurse_live() {

}

int main(int argc, char **argv)
{
	int size, rank, len;
	char processor[100];
	MPI_Comm hobo_world, nurse_world;
	MPI_Group hobo_group, nurse_group;
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor, &len);

	createRole(rank, parseInputValues(argc, argv));

	MPI_Finalize();
	pthread_exit(NULL);
}
