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

struct nurse_data
{
	int if_drunk;
	int weight;
};

struct thread_data
{
	int author;
	int *data_pointer;
	int rank;
	int *lamport;
	MPI_Comm communicator;
};

typedef struct data data;
typedef struct nurse_data nurse_data;
typedef struct thread_data thread_data;

int comparator(const void *first, const void *second) {
	data fdata = *(data*)first;
	data sdata = *(data*)second;
	if (fdata.time == sdata.time)
		return 0;
	return fdata.time > sdata.time ? 1 : -1;
}

int* parseInputValues(int argc, char** argv) {
	static int returned_values[3];
	int hobos_count, nurse_count, hobo_limit;
	if (argc > 2) {
		hobos_count = strtol(argv[1], (char **)NULL, 10);
		nurse_count = strtol(argv[2], (char **)NULL, 10);
		hobo_limit = strtol(argv[3], (char **)NULL, 10);
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
		nurse_live(rank, roles_count[HOBO_INDEX], roles_count[NURSE_INDEX], roles_count[LIMIT_INDEX]);
	} else {
		/*Useless processes*/
		// printf("I have no future... ;( \n");
	}
}

/*Choose lamport value (max of local time and message time)*/
int choose_lamport(int local_time, int message_time) {
	return local_time > message_time ? local_time : message_time;
}

/*Increase lamport value by 1*/
int increment_lamport(int local_time, int message_time) {
	return local_time > message_time ? local_time : message_time;
}

/*Receive a response from other hobo*/
void* receive(void* received_values) {
	MPI_Status st;
	thread_data *dt = (thread_data*) received_values;
	MPI_Recv(dt->data_pointer, 2, MPI_INT, dt->rank, 2, dt->communicator, &st);
	return 0;
}

/*Send request to send_to hobo*/
void* send(void* send_data) {
	thread_data *dt = (thread_data*) send_data;
	MPI_Send(dt->data_pointer, 2, MPI_INT, dt->rank, 11, dt->communicator);
	return 0;
}
void comm_1_4(int hobos_count, thread_data example_data, int *message, int *responses, thread_data *rcv_data, thread_data *snd_data) {
	MPI_Status status;
	/*This threads will be sending access request*/
	pthread_t senders[hobos_count];
	/*This threads will be waiting for responses*/
	pthread_t receivers[hobos_count];
	/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
	int index;
	int hobo_index[hobos_count];
	data request_table[hobos_count];


	for (index = 0; index < hobos_count; index++) {
		if (index == example_data.author)
			continue;

		/*1st phase of communication: send a request*/
		snd_data[index].rank = index;
		snd_data[index].data_pointer = message;
		snd_data[index].communicator = example_data.communicator;
		snd_data[index].author = example_data.author;
		// snd_data[index].lamport = &lamport;
		pthread_create(&senders[index], NULL, send, &snd_data[index]);

		/*4th phase of comunication: collect a responses*/
		rcv_data[index].rank = index;
		rcv_data[index].data_pointer = responses;
		rcv_data[index].communicator = example_data.communicator;
		rcv_data[index].author = example_data.author;
		pthread_create(&receivers[index], NULL, receive, &rcv_data[index]);
	}
}

/*2nd phase of communication: collect requests from all except yourself*/
void comm_2(int hobos_count, data *request_table, MPI_Comm my_comm) {
	MPI_Status status;
	int index, get_message[2];
	for (index = 0; index < hobos_count - 1; index++) {
		MPI_Recv(get_message, 2, MPI_INT, MPI_ANY_SOURCE, 11, my_comm, &status);
		request_table[index].time = get_message[0];
		request_table[index].value = status.MPI_SOURCE;//get_message[1];
		request_table[index].rank = status.MPI_SOURCE;
	}
	request_table[hobos_count -1].time = -1;
	request_table[hobos_count -1].value = -1;
	request_table[hobos_count -1].rank = -1;
}

/*3rd phase of comunication: send responses*/
void comm_3(int hobos_count, data* request_table, int rank, int limit, MPI_Comm my_comm) {
	int index, message[2];
	for (index = 0; index < hobos_count - 1; index++) {
		if (index < limit - 1) {
			message[0] = ACCESS_GRANTED;
			printf("ON: %d Grant access to %d LIMIT: %d\n", rank, request_table[index].rank, limit);
		} else if (index >= limit) {
			message[0] = ACCESS_DENIED;
			printf("ON: %d Denied access to %d\n", rank, request_table[index].rank);
		} else if (index == limit - 1) {
			printf("ON: %d Ignored for now. If I enter I will send to this thread ACCESS Denied %d\n", rank, request_table[index].rank);
			continue;
		}
		MPI_Send(&message, 2, MPI_INT, request_table[index].rank, 2, my_comm);
	}
}

void serve_response(int responses, int rival_rank, MPI_Comm my_comm, int rank) {
	int message[2];
	if (responses == ACCESS_GRANTED) {
		printf("ON: %d I will be in!\n", rank);
		message[0] = ACCESS_DENIED;
	} else {
		printf("ON: %d I will be out!\n", rank);
		message[0] = ACCESS_GRANTED;
	}
	MPI_Send(&message, 2, MPI_INT, rival_rank, 2, my_comm);
}

void wait_for_rescue(int weight, int* nurses_ids, int rank){
	MPI_Status status;
	int message[2];
	while (weight--) {
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
		/*Save ids to responde*/
		if (message[0] == I_WILL_HELP_YOU)
			nurses_ids[weight] = status.MPI_SOURCE;
	}
}
/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count, MPI_Comm my_comm, int nurse_count, int limit) {
	int lamport = 0, index, drunk, weight;
	int message[3] = {ACCESS_REQUEST, 0, 0};
	int nurses_ids[4] = {0};
	MPI_Status status;
	/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
	int responses = 0;
	data request_table[hobos_count];
	thread_data example_data, snd_data[hobos_count], rcv_data[hobos_count];

	example_data.communicator = my_comm;
	example_data.author = rank;
	message[0] = ACCESS_REQUEST;
	message[1] = lamport;

	comm_1_4(hobos_count, example_data, message, &responses, snd_data, rcv_data);

	comm_2(hobos_count, request_table, my_comm);

	qsort(request_table, hobos_count - 1, sizeof(data), comparator);

	comm_3(hobos_count, request_table, rank, limit, my_comm);

	/*Just wait for get response*/
	/*Empty body of this while makes bad things*/
	while (responses == 0) {
		fflush(stdout);}

	serve_response(responses, request_table[limit - 1].rank, my_comm, rank);

	printf("ON: %d After 1st critical section\n", rank);

	MPI_Barrier(my_comm);

	if (responses == ACCESS_GRANTED) {
		printf("ON: %d I am in!\n", rank);
		/*Randomly get drunk*/
		drunk = rand() % 2;
		weight = rand() % 4 + 1;
		message[0] = drunk;
		message[1] = weight;
	} else {
		printf("ON: %d I am out!\n", rank);
		message[0] = 0;
		message[1] = 0;
	}
	index = nurse_count;

	while (index--)
		MPI_Send(&message, 2, MPI_INT, hobos_count + index, 0, MPI_COMM_WORLD);

	printf("ON: %d status: %d and weight: %d\n", rank, drunk, weight);

	if(drunk) {
		// wait_for_rescue(weight, nurses_ids, rank);

		while (weight--) {
			MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
			/*Save ids to responde*/
			if (message[0] == I_WILL_HELP_YOU)
				nurses_ids[weight] = status.MPI_SOURCE;
		}
	}
	if(drunk){
		printf("Hobo: %d Status: I get enough help. I need release my nurses\n", rank);
		message[0] = I_AM_FINE;
		while (drunk && weight != 4) {
			if (nurses_ids[weight])
				MPI_Send(&message, 2, MPI_INT, nurses_ids[weight], 0, MPI_COMM_WORLD);
			weight++;
		}
		printf("Hobo: %d Status: After 2nd critical section\n", rank);
	}
	printf("ON: %d END!\n", rank);
}

/*Recv from all hobos its statuses, return max lamport of all messages*/
int collect_patients(int patients_count, nurse_data *patients_register) {
	int lamport = 0;
	MPI_Status status;
	int recv_message[2] = {0, 0};

		printf("GET INFO\t LIMIT: %d\n", patients_count);
	while (patients_count--) {
		MPI_Recv(&recv_message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		(patients_register + status.MPI_SOURCE)->if_drunk = recv_message[0];
		(patients_register + status.MPI_SOURCE)->weight = recv_message[1];
		if (lamport < recv_message[2])
			lamport = recv_message[2];
	}
	return lamport;
}

/*This function is looking for patient for nurse,
If all nurse are working in this way, they don't need to comunicate*/
int patient_to_help(int my_index, int hobos_count, nurse_data *patients_register) {
	int hobo_index = 0;
	while (hobo_index < hobos_count) {
		/*If not drunk he did not need help*/
		if (!(patients_register + hobo_index)->if_drunk) {
			hobo_index++;
			continue;
		}
		if ((patients_register + hobo_index)->weight - my_index >= 0)
			/*I found my hobo!*/
			return hobo_index;

		/*This mean I am not good enough to help this hobo - I have to low rank*/
		/*I remove weight from my_index beacuse some other nurses will help this hobo,
		so I don't need to rival with them*/
		my_index -= (patients_register + hobo_index)->weight;
		hobo_index++;
	}
	/*No hobo has been found for this nurse*/
	return -1;
}

int help_patients(int rank, int patient_to_help, int lamport) {
	int message[2] = {I_WILL_HELP_YOU, lamport};
	/*Save this hobo!*/
	MPI_Send(&message, 2, MPI_INT, patient_to_help, 0, MPI_COMM_WORLD);
	printf("Nurse %d I wait for OK status from current hobo: %d\n", rank, patient_to_help);
	/*Before you go further, wait for acknowlege!*/
	MPI_Recv(&message, 2, MPI_INT, patient_to_help, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	if (message[0] == I_AM_FINE) {
		printf("Nurse %d did help hobo with rank: %d\n", rank, patient_to_help);
	} else {
		exit(1);
	}
}
/*Find and help all hobos you should help to
Another possible name for this function: find_and_cure*/
void nurse_job(int rank, int hobos_count, int nurse_count, nurse_data *patients, int lamport) {
	int offset = 0;
	int my_index, hobo_to_cure;
	int index = hobos_count;
	while (index--) {
		/*my index it's a relative index inside nurses group
		WARNING: my index will have value in range: <1..nurse_count>*/
		my_index = rank - hobos_count + 1 + offset * nurse_count;

		hobo_to_cure = patient_to_help(my_index, hobos_count, patients);

		if (hobo_to_cure > -1) {
			help_patients(rank, hobo_to_cure, lamport);
			offset++;
		} else {
			/*if current my_index isn't good enough there's no sense to looking for another hobo*/
			break;
		}
	}
}
/*This function will be executed by Nurses processes*/
int nurse_live(int rank, int hobos_count, int nurse_count, int limit) {
	int index = hobos_count;
	int offset = 0;
	int lamport = 0;
	nurse_data patients[hobos_count];
	while(index--)
		patients[index].if_drunk = 0;
		patients[index].weight = 0;
	
	lamport = collect_patients(hobos_count, patients);
	printf("Nurse %d I get info from all\n", rank);
	/*
	Now nurse know all about each hobo: what is its weight or if he is drunk
	So this is time to respond on help request: nurse needs to help hobo.
	Because each nurse has the same data, nurses will be NOT communicate with each other.
	Simply they send a message to specific hobo. This will be mean, this nurse is helping this hobo.
	*/

	nurse_job(rank, hobos_count, nurse_count, patients, lamport);
	printf("Nurse: %d END\n", rank);
}

int main(int argc, char **argv)
{
	int size, rank, len, out;
	char processor[100];
	MPI_Comm hobo_world, nurse_world;
	MPI_Group hobo_group, nurse_group;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &out);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor, &len);

	srand(time(NULL) + rank);
	createRole(rank, parseInputValues(argc, argv));

	MPI_Finalize();
	pthread_exit(NULL);
	exit(0);
}
