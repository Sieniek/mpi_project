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

#define HOBO_INDEX 0
#define NURSE_INDEX 1
#define LIMIT_INDEX 2
#define ACCESS_GRANTED 2
#define ACCESS_DENIED 1
#define ACCESS_REQUEST 9

#define PROBABILITY 0.5

#define I_WILL_HELP_YOU 123
#define I_AM_FINE 321
#define REPEAT_COUNT 24


#define DRUNK 0
#define WEIGHT 1

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

int lamport_value = 0;

/*Increment and return lamport value*/
int get_lamport_original(){
	int local_time;
	
	local_time = lamport_value;

	return local_time;
}
/*Increment and return lamport value*/
int get_lamport(){
	int local_time; 
	lamport_value++;
	local_time = lamport_value;
	return local_time;
}

/*Replace lamport value if time message is bigger*/
void set_lamport(int value){
	lamport_value = value > lamport_value ? value : lamport_value;
}

int comparator(const void *first, const void *second) {
	data fdata = *(data*)first;
	data sdata = *(data*)second;
	if (fdata.time == sdata.time)
		return fdata.rank > sdata.rank ? 1 : -1;
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
		hobo_live(rank, roles_count[HOBO_INDEX], hobo_comm, roles_count[NURSE_INDEX], roles_count[LIMIT_INDEX]);
	} else if (rank < roles_count[HOBO_INDEX] + roles_count[NURSE_INDEX]) {
		/*Nurse*/
		nurse_live(rank, roles_count[HOBO_INDEX], roles_count[NURSE_INDEX], roles_count[LIMIT_INDEX]);
	} else {
		/*Useless processes*/
	}
}

void comm_1_4(int hobos_count, thread_data example_data, int *message, int *responses, MPI_Request *request_statuses) {
	/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
	int index;
	get_lamport();

	MPI_Request request, request_recv;
	for (index = 0; index < hobos_count; index++) {
		if (index == example_data.author){
			request_statuses[index] = MPI_REQUEST_NULL;
			continue;
		}
		MPI_Isend(message, 2, MPI_INT, index, 11, example_data.communicator, &request);
		MPI_Irecv(responses + index * 2, 2, MPI_INT, index, 2, example_data.communicator, request_statuses + index);
	}
}

/*2nd phase of communication: collect requests from all except yourself*/
void comm_2(int hobos_count, data *request_table, MPI_Comm my_comm) {
	MPI_Status status;
	int index, get_message[2];
	for (index = 0; index < hobos_count - 1; index++) {
		MPI_Recv(get_message, 2, MPI_INT, MPI_ANY_SOURCE, 11, my_comm, &status);
		request_table[index].value = get_message[0];
		request_table[index].time = get_message[1];
		request_table[index].rank = status.MPI_SOURCE;
	}
	set_lamport(get_message[1]);
	request_table[hobos_count -1].time = -1;
	request_table[hobos_count -1].rank = -1;
	request_table[hobos_count -1].value = -1;
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
		message[1] = get_lamport();
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
	message[1] = get_lamport();
	MPI_Send(&message, 2, MPI_INT, rival_rank, 2, my_comm);
}

void wait_for_rescue(int weight, int* nurses_ids, int rank){
	MPI_Status status;
	int message[2];
	while (weight--) {
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
		/*Save ids to response*/
		if (message[0] == I_WILL_HELP_YOU)
			nurses_ids[weight] = status.MPI_SOURCE;
	}
}

int first_critical_section(int rank, MPI_Comm my_comm, int hobos_count, int limit, int *responses){

		int index, drunk, weight;
		int out_index;
		int message[3] = {ACCESS_REQUEST, 0, 0};
		MPI_Status status;
		/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
		data request_table[hobos_count];
		thread_data example_data;
		MPI_Request request_statuses[hobos_count];

		example_data.communicator = my_comm;
		example_data.author = rank;

		comm_1_4(hobos_count, example_data, message, responses, request_statuses);

		comm_2(hobos_count, request_table, my_comm);

		qsort(request_table, hobos_count - 1, sizeof(data), comparator);

		comm_3(hobos_count, request_table, rank, limit, my_comm);

		/*Just wait for any response*/
		MPI_Waitany(hobos_count, request_statuses, &out_index, &status);
		printf("Response from: %d", out_index);
		
		serve_response(responses[out_index * 2], request_table[limit - 1].rank, my_comm, rank);

		return out_index;
}

/*This function will be executed by Hobos processes*/
int hobo_live(int rank, int hobos_count, MPI_Comm my_comm, int nurse_count, int limit) {
	int cycle_count = 0;
	while(cycle_count++ < REPEAT_COUNT){

		int index, drunk, weight, out_index;
		int message2[3] = {ACCESS_REQUEST, 0, 0};
		int message3[3] = {ACCESS_REQUEST, 0, 0};
		int nurses_ids[4] = {0,0,0,0};
		int responses[hobos_count * 2];
		MPI_Status status;

		printf("=====================\n");
		printf("========..%d..=======\n",cycle_count);
		printf("=====================\n");

		out_index = first_critical_section(rank, my_comm, hobos_count, limit, responses);

		printf("ON: %d at %d After 1st critical section\n", rank, lamport_value);

		// if (responses[out_index * 2] == ACCESS_GRANTED) {
		// 	printf("ON: %d I am in!\n", rank);
		// 	/*Randomly get drunk*/
		// 	drunk = rand() % 2;
		// 	weight = rand() % 4 + 1;
		// 	message2[0] = drunk;
		// 	message2[1] = weight;
		// 	printf("ON: %d status: %d and weight: %d\n", rank, drunk, weight);
		// } else {
		// 	printf("ON: %d I am out!\n", rank);
		// 	drunk = 0;
		// 	message2[0] = 0;
		// 	message2[1] = 0;
		// }
		// 	printf("Hobo: %d X0X: %d %d \n", rank, message2[0], message2[1]);
		int stat = responses[out_index * 2];
		party(rank, message2, stat);
		index = nurse_count;

			printf("Hobo: %d XXX: %d %d \n", rank, message2[0], message2[1]);
		while (index--){
			message2[2] = get_lamport();
			MPI_Send(&message2, 3, MPI_INT, hobos_count + index, 0, MPI_COMM_WORLD);
		}


		if(message2[0]) {
			get_nurses(message2[1], rank, nurses_ids);
			printf("Hobo: %d Status: I get enough help. I need release my nurses\n", rank);
			release_nurses(rank, nurses_ids);
			printf("Hobo: %d Status: After 2nd critical section\n", rank);
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}
	printf("ON: %d END!\n", rank);
}

int party(int rank, int *message2, int status){
	int drunk, weight;
	if (status == ACCESS_GRANTED) {
		printf("ON: %d I am in!\n", rank);
		/*Randomly get drunk*/
		message2[0] = rand() % 2;
		message2[1] = rand() % 4 + 1;
		printf("ON: %d status: %d and weight: %d\n", rank, drunk, weight);
	} else {
		printf("ON: %d I am out!\n", rank);
		drunk = 0;
		message2[0] = 0;
		message2[1] = 0;
	}
}

int release_nurses(int rank, int *nurses_ids){
	int index = 0;
	int message[2] = {I_AM_FINE, 0};
	while (index != 4) {
		if (nurses_ids[index]){
			message[1] = get_lamport();
			MPI_Send(&message, 2, MPI_INT, nurses_ids[index], 0, MPI_COMM_WORLD);
			printf("Hobo: %d releaed %d\n", rank, nurses_ids[index]);
		}
		index++;
	}
}

int get_nurses(int weight, int rank, int *nurses_ids){
	int message[2];
	MPI_Status status;
	while (weight--) {
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		printf("Hobo %d receive help from: %d\n", rank, status.MPI_SOURCE);
		// Save ids to responde
		if (message[0] == I_WILL_HELP_YOU){
			set_lamport(message[1]);
			nurses_ids[weight] = status.MPI_SOURCE;
		}else{
			exit(2);
		}
	}
}

/*Recv from all hobos its statuses, return max lamport of all messages*/
int collect_patients(int patients_count, nurse_data *patients_register) {
	int lamport = 0;
	MPI_Status status;
	int recv_message[2] = {0, 0};
	while (patients_count--) {
		MPI_Recv(&recv_message, 3, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		(patients_register + status.MPI_SOURCE)->if_drunk = recv_message[0];
		(patients_register + status.MPI_SOURCE)->weight = recv_message[1];
	
		set_lamport(recv_message[2]);
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

/*
Communication between specific hobo: patient and nurse
Firstly nurse send message I_WILL_HELP_YOU to patient
Then she needs to wait for his response I_AM_FINE
*/
int help_patients(int rank, int patient_to_help, int lamport) {
	int message[2] = {I_WILL_HELP_YOU, get_lamport()};
	/*Save this hobo!*/
	MPI_Send(&message, 2, MPI_INT, patient_to_help, 0, MPI_COMM_WORLD);
	printf("Nurse %d I wait for OK status from current hobo: %d\n", rank, patient_to_help);
	/*Before you go further, wait for acknowlege!*/
	MPI_Recv(&message, 2, MPI_INT, patient_to_help, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	if (message[0] == I_AM_FINE) {
		printf("Nurse %d did help hobo with rank: %d\n", rank, patient_to_help);
		set_lamport(message[1]);
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

/* Here all variables used by nurse process will be set to default values*/
void init_nurse_variables(int *offset, nurse_data *patients, int index){
	(*offset) = 0;

	while(index--){
		patients[index].if_drunk = 0;
		patients[index].weight = 0;
	}
}
/*This function will be executed by Nurses processes*/
int nurse_live(int rank, int hobos_count, int nurse_count, int limit) {
	int cycle_count = 0;
	int index = hobos_count;
	int offset = 0;
	int lamport = 0;
	nurse_data patients[hobos_count];

	while(cycle_count++ < REPEAT_COUNT){

		printf("=====================\n");
		printf("NUR=====..%d..=======\n",cycle_count);
		printf("=====================\n");

		printf("Nurse %d I wait for info from all\n", rank);
		init_nurse_variables(&offset, patients, hobos_count);
		lamport = collect_patients(hobos_count, patients);
		printf("Nurse %d I get info from all\n", rank);
		/*
		Now nurse know all about each hobo: what is its weight or if he is drunk
		So this is time to respond on help request: nurse needs to help hobo.
		Because each nurse has the same data, nurses will be NOT communicate with each other.
		They simply send a message to specific hobo. This will be mean, this nurse is helping this hobo.
		*/

		nurse_job(rank, hobos_count, nurse_count, patients, lamport);

		MPI_Barrier(MPI_COMM_WORLD);
	}
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
