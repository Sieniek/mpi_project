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

/*Data needed to comunicate: senderID - rank of thread's parent process,
receiverID - id of hobo to comunicate with,
response - code of response
in future it will be also timer to trac comunications in program*/
struct ThreadData {
	int senderID;
	int receiverID;
	int response;
	int timer;
};
typedef struct ThreadData ThreadData;

/*Comunication between two hobos*/
void *comunication(void *inOutParameter) {
	ThreadData comunicationData = *((ThreadData*) inOutParameter);

	pthread_t send_request, response_for_request;



	if ( pthread_create(&send_request, NULL, comunication, (void*) &communication_data[hobo_index]) != 0)
			exit(-1);
	/*Split to two roles*/
	// if (fork()) {
	// 	/*It's receiver. This role is wait for request for access for it's destination hobo*/

	// } else {
	// 	/*It's sender. This role is sending request for access for it's hobo - parent process*/

	// }
	if(comunicationData.senderID > comunicationData.receiverID){
		/*Send request and wait for response*/
		int send_result = MPI_Send(&comunicationData.senderID, 1, MPI_INT, comunicationData.receiverID, 0, MPI_COMM_WORLD);
		int recv_result = MPI_Recv(&comunicationData.response, 1, MPI_INT, comunicationData.receiverID, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		printf("Comunication: From %d to %d RESPONSE: %d \n", comunicationData.senderID, comunicationData.receiverID, comunicationData.response);	
	}else{
		/*Wait for request and send response*/
		int response = 900;
		int recv_result = MPI_Recv(&comunicationData.response, 1, MPI_INT, comunicationData.receiverID, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		if(comunicationData.receiverID % 3)
			response = 14;
		int send_result = MPI_Send(&response, 1, MPI_INT, comunicationData.receiverID, 0, MPI_COMM_WORLD);
	}
	pthread_exit(NULL);
}

void *sendAccessRequest(void *inOutParameter)
{	/*
	int hoboToAskId = *((int *) inOutParameter);

	MPI_Send(
	    void* data,
	    int count,
	    MPI_Datatype datatype,
	    int destination,
	    int tag,
	    MPI_Comm communicator);
	int *add = (int *) threadid;
	*add = 0;*/
	pthread_exit(NULL);
}

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
	if (rank < roles_count[HOBO_INDEX]) {
		/*Hobo*/
		printf("I will be hobo in future! :D \n");
		hobo_live(rank, roles_count[HOBO_INDEX]);
	} else if (rank < roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX]) {
		/*Nurse*/
		printf("I will be nurse in future. :| \n");
		nurse_live();
	} else {
		/*Useless processes*/
		printf("I have no future... ;( \n");
	}
}

/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count) {
	/*Variable to indexing*/
	int hobo_index;

	/*Table of threads. Each thread to ask another hobo asynchronymus*/
	pthread_t threads[hobos_count];

	/*Table with data to comunication for this hobo to all hobos*/
	ThreadData communication_data[hobos_count];

	/*Allow access to myself by myself*/
	communication_data[rank].response = ACCESS_GRANTED;

	/*For each hobo...*/
	for (hobo_index = 0; hobo_index < hobos_count; hobo_index++) {
		/*It's myself, go to next hobo*/
		if (hobo_index == rank)
			continue;

		/*initialize each struct with rank as sender and ACCESS_DENIED as response*/
		communication_data[hobo_index].response = ACCESS_DENIED;
		communication_data[hobo_index].senderID = rank;
		communication_data[hobo_index].receiverID = hobo_index;
		/*Create thread to ask another hobo for place. Set fuction to run by new thread: PrintHello
		and send place for response: address of response[t]. Check if creating is finish with success: return code == 0,
		if not fail program*/
		if ( pthread_create(&threads[hobo_index], NULL, comunication, (void*) &communication_data[hobo_index]) != 0)
			exit(-1);
	}
	/*After you run all threads to asking hobos about access check responses.
	Set counter to 0 if you see someone is not allowing you to enter*/
	for (hobo_index = 0; hobo_index < hobos_count; hobo_index++)
		if (communication_data[hobo_index].response != ACCESS_GRANTED)
			hobo_index = 0;

	printf("I'm in! \n");
}

/*This function will be executed by Nurses processes*/
int nurse_live() {

}

int main(int argc, char **argv)
{

	int size, rank, len;
	char processor[100];
	int msg = 4;

	/*This is local timer value of Lamport's timer*/
	int lamport = 0;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor, &len);

	createRole(rank, parseInputValues(argc, argv));

	MPI_Finalize();
	pthread_exit(NULL);
}


