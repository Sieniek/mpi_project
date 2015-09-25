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
#define LIMIT_INDEX 2
#define ACCESS_GRANTED 2
#define ACCESS_DENIED 1
#define ACCESS_REQUEST 9

#define PROBABILITY 0.5

#define I_WILL_HELP_YOU 123
#define I_AM_FINE 321

struct data
{
	int time;
	int rank;
	int value;
};

struct thread_data
{
	int *data_pointer;
	int rank;
	MPI_Comm *communicator;
};

typedef struct data data;
typedef struct thread_data thread_data;

int comparator(const void *first, const void *second){
	data *fdata = (data*)first;
	data *sdata = (data*)second;
	return fdata->time > sdata->time ? 1 : -1;
}

int* parseInputValues(int argc, char** argv) {
	static int returned_values[3];
	int hobos_count, nurse_count, hobo_limit;
	if (argc > 2) {
		hobos_count = strtol(argv[1], (char **)NULL, 10);
		nurse_count = strtol(argv[2], (char **)NULL, 10);
		hobo_limit = strtol(argv[2], (char **)NULL, 10);
		printf("Hobos: %d, Nurses: %d\n", hobos_count, nurse_count);
	} else {
		printf("No parameters set! Default parameters will be used: hobos=100 and nurse=10\n");
		hobos_count = 100;
		nurse_count = 10;
		hobo_limit = 50;
	}
	returned_values[0] = hobos_count;
	returned_values[1] = nurse_count;
	returned_values[2] = hobo_limit;
	return returned_values;
}

/*Set process to different roles*/
void createRole(int rank, int* roles_count) {
	MPI_Comm hobo_comm, nurse_comm;
	MPI_Comm_split(MPI_COMM_WORLD, rank < roles_count[HOBO_INDEX], 0, &hobo_comm);
	MPI_Comm_split(MPI_COMM_WORLD, rank <  roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX], 0, &nurse_comm);


	if (rank < roles_count[HOBO_INDEX]) {
		/*Hobo*/
		// printf("I will be hobo in future! :D %d \n", roles_count[HOBO_INDEX]);
		hobo_live(rank, roles_count[HOBO_INDEX], hobo_comm, roles_count[NURSE_INDEX], roles_count[LIMIT_INDEX]);
	} else if (rank < roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX]) {
		/*Nurse*/
		// printf("I will be nurse in future. :| \n");
		nurse_live(rank, roles_count[HOBO_INDEX], roles_count[NURSE_INDEX]);
	} else {
		/*Useless processes*/
		// printf("I have no future... ;( \n");
	}
}

int update_lamport(int local_time, int message_time) {
	return local_time > message_time ? local_time : message_time;
}

/*Receive a response from other hobo*/
void* receive(void* received_values){
	thread_data *dt = (thread_data*) received_values;
	MPI_Recv(dt->data_pointer, 2, MPI_INT, MPI_ANY_SOURCE, 0, *(dt->communicator), MPI_STATUS_IGNORE);
	return NULL;
}

/*Send request to send_to hobo*/
void* send(void* send_data){
	thread_data *dt = (thread_data*) send_data;
	MPI_Send(dt->data_pointer, 2, MPI_INT, dt->rank, 0, *(dt->communicator));
	printf("Sended to %d  value",dt->rank);
	return NULL;
}
/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count, MPI_Comm my_comm, int nurse_count, int limit) {

	int lamport = 0, index, drunk, weight;
	int message[2];
	int get_message[2], recv_buff[2 * hobos_count];
	int nurses_ids[4] = {0};
	MPI_Status status;
	/*This threads will be sending access request*/
	pthread_t senders[hobos_count];
	/*This threads will be waiting for responses*/
	pthread_t receivers[hobos_count];
	/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
	int responses = 0;
	int hobo_index[hobos_count];
	data request_table[hobos_count-1];

	thread_data snd_data[hobos_count];
	thread_data rcv_data[hobos_count];

	message[0] = ACCESS_REQUEST;
	message[1] = lamport;
	/*Allgather will send request to all and gather others requests*/
	// MPI_Allgather(&message, 2, MPI_INT, recv_buff, 1, MPI_INT, my_comm);

	printf("ON: %d Before com1\n", rank);
	for(index = 0; index < hobos_count; index++){
		if(index == rank) 
			continue;
		hobo_index[index] = index;
		/*1st phase of communication: send a request*/
		snd_data[index].rank = index;
		snd_data[index].data_pointer = message;
		snd_data[index].communicator = &my_comm;

		printf("ON: %d Send request to %d\n", rank, index);
		pthread_create(&senders[index], NULL, send, &snd_data[index]);
		/*4th phase of comunication: collect a responses*/
		rcv_data[index].data_pointer = &responses;
		rcv_data[index].communicator = &my_comm;
		pthread_create(&receivers[index], NULL, receive, &rcv_data[index]);
	}

	printf("ON: %d After threads\n", rank);
	/*2nd phase of communication: collect requests from all except yourself*/
	for(index = 0; index < hobos_count - 1; index++){
		MPI_Recv(&get_message, 2, MPI_INT, MPI_ANY_SOURCE, 0, my_comm, &status);
		request_table[index].time = get_message[0];
		request_table[index].value = get_message[1]; 
		request_table[index].rank = status.MPI_SOURCE; 
		printf("ON: %d Received request from\t %d \t\n", rank, status.MPI_SOURCE);
	}

	printf("ON: %d Before sort\n", rank);
	qsort(request_table, hobos_count - 1, sizeof(data), comparator);
	/*3rd phase of comunication: send responses*/
	for(index = 0; index < hobos_count - 1; index++){
		//tu powinna byc posortowana lista

		
		if(index < limit - 1){
			message[0] = ACCESS_GRANTED;

	printf("ON: %d Grant access to %d\n", rank, request_table[index].rank);
		}else if(index > limit){
			message[0] = ACCESS_DENIED;
	printf("ON: %d Denied access to %d\n", rank, request_table[index].rank);
		}else{
	printf("ON: %d Ignored %d\n", rank, request_table[index].rank);
			continue; 
		}
		// message[0] = ACCESS_GRANTED;
		MPI_Send(&message, 2, MPI_INT, request_table[index].rank, 0, my_comm);	
	}

	printf("ON: %d Before wait for responses\n", rank);
	/*Just wait for get response*/
	while(!responses){}

	printf("ON: %d After 1st critical section\n", rank);
	MPI_Barrier(my_comm);
	if(responses == ACCESS_GRANTED){
		printf("I am in!");
	
		/*Randomly get drunk*/
		drunk = rand() % 2;
		weight = rand() % 4 + 1;
		index = nurse_count;
		message[0] = drunk;
		message[1] = weight;
		while (index--)
			MPI_Send(&message, 2, MPI_INT, hobos_count + index, 0, MPI_COMM_WORLD);

		printf("ON: %d status: %d and weight: %d\n", rank, drunk, weight);

		while (drunk && weight--) {
			MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
			/*Save ids to responde*/
			if (message[0] == I_WILL_HELP_YOU)
				nurses_ids[weight] = status.MPI_SOURCE;
		}

		printf("Hobo: %d Status: I get enough help. I need release my nurses\n", rank);
		message[0] = I_AM_FINE;
		while (drunk && weight != 4) {
			if (nurses_ids[weight])
				MPI_Send(&message, 2, MPI_INT, nurses_ids[weight], 0, MPI_COMM_WORLD);
			weight++;
		}
		printf("Hobo: %d Status: After 2nd critical section\n", rank);
	}else{
		printf("I am out!");
	}
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
	while (index--) {
		MPI_Recv(&recv_message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if (rank == 8)
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
	while (index--) {
		/*my index it's a relative index inside nurses group
		WARNING: my index will have value in range: <1..nurse_count>*/
		int my_index = rank - hobos_count + 1 + offset * nurse_count;
		int hobo_index = 0, hobo_to_send = -1;
		while (hobo_index < hobos_count) {
			/*If not drunk he did not need help*/
			if (!if_drunk[hobo_index]) {
				hobo_index++;
				continue;
			}
			if (weights[hobo_index] - my_index >= 0) {
				/*I find my hobo!*/
				hobo_to_send = hobo_index;
				break;
			} else {
				/*this mean I am not good enough to help this hobo - I have to low rank*/
				/*I remove weight from my_index beacuse some other nurses will help this hobo,
				so I don't need to concure with them*/
				my_index -= weights[hobo_index];
				hobo_index++;

			}

		}
		if (hobo_to_send > -1) {
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
