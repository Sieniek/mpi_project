
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

#define NUM_THREADS     5
#define HOBO_INDEX 0
#define NURSE_INDEX 1
#define ACCESS_GRANTED 0
#define ACCESS_DENIED 1

void *PrintHello(void *threadid)
{
	int a = *((int *) threadid);

	printf("Hello World! It's me, thread! ARG= %d\n", a);
	int *add = (int *) threadid;
	*add = 0;
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
	/*Table of threads. Each thread to ask another hobo asynchronymus*/
	pthread_t threads[hobos_count];
	/*Array to keep responses*/
	int responses[hobos_count] = {ACCESS_DENIED};
	/*Allow access to myself by myself*/
	responses[rank] = ACCESS_GRANTED;

	/*For each hobo...*/
	for (hobo_index = 0; t < hobos_count; hobo_index++) {
		/*It's myself, go to next hobo*/
		if (hobo_index == rank)
			continue;

		/*Set response field to id of hobo*/
		responses[hobo_index] = hobo_index
		/*Create thread to ask another hobo for place. Set fuction to run by new thread: PrintHello
		and send place for response: address of response[t]. Check if creating is finish with success: return code == 0,
		if not fail program*/
		if (pthread_create(&threads[hobo_index], NULL, PrintHello, &responses[hobo_index]); != 0)
			exit(-1);
	}
	/*After you run all threads to asking hobos about access check responses.
	Set counter to 0 if you see someone is not allowing you to enter*/
	for (hobo_index = 0; t < hobos_count; hobo_index++)
		if (responses[hobo_index] != ACCESS_GRANTED)
			hobo_index = 0;

	printf("I'm in! \n");

	// for (t = 0; t < NUM_THREADS; t++)
	// 	printf("All zeros! t= %d \t arg[t] = %d\n", t, arg[t]);



	// if (fork()) { //try enter
	// 	int hobo_id;
	// 	//for(hobo_id = 0; hobo_id < hobos_count; hobo_id++)
	// 	//	if(hobo_id != rank)
	// 	//	MPI_Bcast(&msg, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
	// 	//printf("Otrzymano msg = %d na procesie o id = %d\n", msg, rank);
	// } else { //send a response to the others
	// 	//MPI_Bcast(&msg, 1, MPI_FLOAT, rank, MPI_COMM_WORLD);
	// }
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


