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
#include <time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define HOBO_INDEX 0
#define NURSE_INDEX 1
#define ACCESS_GRANTED 0
#define ACCESS_DENIED 1
#define ACCESS_REQUEST 9

#define PROBABILITY 0.5

#define I_WILL_HELP_YOU 123
#define I_AM_FINE 321

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
		hobo_live(rank, roles_count[HOBO_INDEX], hobo_comm, roles_count[NURSE_INDEX]);
	} else if (rank < roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX]) {
		/*Nurse*/
		printf("I will be nurse in future. :| \n");
		nurse_live(rank, roles_count[HOBO_INDEX], roles_count[NURSE_INDEX]);
	} else {
		/*Useless processes*/
		printf("I have no future... ;( \n");
	}
}

int update_lamport(int local_time, int message_time){
	return local_time > message_time ? local_time : message_time;
}

/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count, MPI_Comm my_comm, int nurse_count) {

	int lamport;
	int message[2] = {ACCESS_REQUEST, lamport};
	int get_message[2];
	int recv_buff[hobos_count], index;
	int drunk;
	MPI_Status status;
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
	
	/*Not required except if after 1 section processes should go further together*/
	// MPI_Barrier(my_comm);

	/*Randomly get drunk*/
	drunk = rand() % 2;
	int weight = rand() % 4 + 1;
	index = nurse_count;
	message[0] = drunk;
	message[1] = weight;
	int nurses_ids[4] = {0};
	while(index--)
		MPI_Send(&message, 2, MPI_INT, hobos_count + index, 0, MPI_COMM_WORLD);

	printf("ON: %d status: %d and weight: %d\n", rank, drunk, weight);

	while(drunk && weight--){
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
		/*Save ids to responde*/
		if(message[0] == I_WILL_HELP_YOU)
			nurses_ids[weight] = status.MPI_SOURCE;
	}

	printf("Hobo: %d Status: I get enough help. I need release my nurses\n", rank);
	message[0] = I_AM_FINE;
	while(drunk && weight != 4){
		if(nurses_ids[weight])
			MPI_Send(&message, 2, MPI_INT, nurses_ids[weight], 0, MPI_COMM_WORLD);
		weight++;
	}
		
	printf("Hobo: %d Status: After 2nd critical section\n", rank);

}

/*This function will be executed by Nurses processes*/
int nurse_live(int rank, int hobos_count, int nurse_count) {
	int index = hobos_count;
	int if_drunk[hobos_count];
	int weights[hobos_count];
	int recv_message[2] = {0, 0};
	int send_message[2] = {0, 0};
	MPI_Status status;

	/*Recv from all hobos its statuses*/
	while(index--){
		MPI_Recv(&recv_message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if(rank == 8)
			printf("FROM: %d status: %d and weight: %d\n", status.MPI_SOURCE, recv_message[0], recv_message[1]);
		if_drunk[status.MPI_SOURCE] = recv_message[0];
		weights[status.MPI_SOURCE] = recv_message[1];
	}
	printf("Nurse %d I get info from all\n", rank);
	/*
	Now nurse know all about each hobo: what is its weight or if he is drunk
	So this is time to respond on help request: nurse needs to help hobo.
	Because each nurse has the same data, nurses will be NOT communicate with each other.
	Simply they send a message to specific hobo. This will be mean, this nurse is helping this hobo.
	*/
	int offset = 0;
	index = hobos_count;
	send_message[0] = I_WILL_HELP_YOU;
	while(index--){
			/*my index it's a relative index inside nurses group
			WARNING: my index will have value in range: <1..nurse_count>*/
		int my_index = rank - hobos_count + 1 + offset * nurse_count;
		int hobo_index = 0, hobo_to_send = -1;
		while(hobo_index < hobos_count){
			/*If not drunk he did not need help*/
			if(!if_drunk[hobo_index]){
				hobo_index++;
				continue;
			}
			if(weights[hobo_index] - my_index >= 0){
				/*I find my hobo!*/
				hobo_to_send = hobo_index;
				break;
			}else{
				/*this mean I am not good enough to help this hobo - I have to low rank*/
				/*I remove weight from my_index beacuse some other nurses will help this hobo,
				so I don't need to concure with them*/
				my_index -= weights[hobo_index];
				hobo_index++;
				
			}

		}
		if(hobo_to_send > -1){
			printf("Nurse %d choosed hobo with rank: %d\n", rank, hobo_index);
			/*Save this hobo!*/
			MPI_Send(&send_message, 2, MPI_INT, hobo_to_send, 0, MPI_COMM_WORLD);

			printf("Nurse %d I wait for OK status from current hobo: %d\n", rank, hobo_to_send);
			/*Before you go further, wait for acknowlege!*/
			MPI_Recv(&recv_message, 2, MPI_INT, hobo_to_send, 0, MPI_COMM_WORLD, &status);
			printf("Nurse %d did help hobo with rank: %d\n", rank, hobo_to_send);
			offset++;
		}

	}

	printf("Nurse: %d koniec na nurse\n", rank);

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

	srand(time(NULL) + rank);
	createRole(rank, parseInputValues(argc, argv));

	MPI_Finalize();
	pthread_exit(NULL);
}
