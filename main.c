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

#define NO_ACCESS_REQUEST 1415

#define PROBABILITY 0.5

#define I_WILL_HELP_YOU 123
#define I_AM_FINE 321

#define DRUNK 0
#define WEIGHT 1

struct basic_parameters{
	int hobos_count;
	int nurse_count;
	int hobo_limit;
	int loop_limit;
	int rank;
	int lamport;
	MPI_Comm my_comm;
};

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
typedef struct basic_parameters basic_parameters;
typedef struct data data;
typedef struct nurse_data nurse_data;

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
	if (fdata.time < 0){
		return 1;
	}
	if (sdata.time < 0){
		return -1;
	}
	if (fdata.time == sdata.time)
		return fdata.rank > sdata.rank ? 1 : -1;
	return fdata.time > sdata.time ? 1 : -1;
}

basic_parameters* parseInputValues(int argc, char** argv) {

	static basic_parameters returned_values;
	// static int returned_values[3];
	int hobos_count, nurse_count, hobo_limit, loop_limit;
	if (argc > 3) {
		hobos_count = strtol(argv[1], (char **)NULL, 10);
		nurse_count = strtol(argv[2], (char **)NULL, 10);
		hobo_limit = strtol(argv[3], (char **)NULL, 10);
		loop_limit = strtol(argv[4], (char **)NULL, 10);
		printf("Hobos: %d, Nurses: %d Limit: %d, Loops: %d\n", hobos_count, nurse_count, hobo_limit, loop_limit);
	} else {
		printf("No parameters set! Default parameters will be used: hobos=100 and nurse=10\n");
		hobos_count = 6;
		nurse_count = 4;
		hobo_limit = 3;
		loop_limit = 10;
	}
	returned_values.hobos_count = hobos_count;
	returned_values.nurse_count = nurse_count;
	returned_values.hobo_limit = hobo_limit;
	returned_values.loop_limit = loop_limit;
	return &returned_values;
}

/*Set process to different roles*/
void createRole(int rank, basic_parameters* parameters) {
	MPI_Comm hobo_comm;
	MPI_Comm_split(MPI_COMM_WORLD, rank < parameters->hobos_count, 0, &hobo_comm);

	parameters->rank = rank;

	if (rank < parameters->hobos_count) {
		/*Hobo*/
		parameters->my_comm = hobo_comm;
		printf("HOBO id %d!!!\n", parameters->rank);
		hobo_live(parameters);
	} else if (rank < parameters->hobos_count + parameters->nurse_count) {
		/*Nurse*/

		printf("NURSE id %d!!!\n", parameters->rank);
		nurse_live(parameters);
	} else {
		/*Useless processes*/
		printf("Process ENDED!!!\n");
	}
}

void comm_1_4(basic_parameters *parameters, int *responses, MPI_Request *error_request, data *request_table) {
	int message[3] = {ACCESS_REQUEST, 0, 0};
	int index;
	int lamport = get_lamport();
	int error_message[2] = {0, 0};

	if(0){
		error_message[0] = NO_ACCESS_REQUEST;
	}

	MPI_Request request, request_recv;
	for (index = 0; index < parameters->hobos_count; index++) {
		request_table[index].time = -1;
		if (index == parameters->rank){
			error_request[index] = MPI_REQUEST_NULL;
			continue;
		}
		message[1] = lamport;
		MPI_Isend(message, 2, MPI_INT, index, 11, parameters->my_comm, &request);
		MPI_Isend(message, 2, MPI_INT, index, 99, parameters->my_comm, error_request + index);
		// MPI_Irecv(responses + index * 2, 2, MPI_INT, index, 2, parameters->my_comm, request_statuses + index);
	}

	request_table[parameters->rank].time = lamport;
	request_table[parameters->rank].rank = parameters->rank;
	request_table[parameters->rank].value = ACCESS_REQUEST;
}

/*2nd phase of communication: collect requests from all except yourself*/
void comm_2(basic_parameters *parameters, data *request_table) {
	MPI_Status status;
	int index = parameters->hobo_limit, get_message[2];
	int error_message[2];
	//wait for minimum requests
	printf("wait for limit\n");
	while (index--) {
		MPI_Recv(get_message, 2, MPI_INT, MPI_ANY_SOURCE, 11, parameters->my_comm, &status);
		request_table[status.MPI_SOURCE].value = get_message[0];
		request_table[status.MPI_SOURCE].time = get_message[1];
		request_table[status.MPI_SOURCE].rank = status.MPI_SOURCE;
	}
	//ask others
	printf("On %d get info from %d processes. I will ask others...\n", parameters->rank, parameters->hobo_limit);
	index = parameters->hobos_count;
	while(index--){
		if(request_table[index].time != -1)
			continue;
		
		//ask aboit it's request
		printf("On %d I will ask %d\n", parameters->rank, index);
		MPI_Recv(error_message, 2, MPI_INT, index, 99, parameters->my_comm, MPI_STATUS_IGNORE);
		if(error_message[0] == NO_ACCESS_REQUEST){
			//Process doesn't send a request

			printf("On %d Process %d didn't send q request\n", parameters->rank, index);
		}else{ // else: process send a request, so I need to recv it
			printf("On %d Process %d DID send q request, I will wait for it\n", parameters->rank, index);
			MPI_Recv(get_message, 2, MPI_INT, index, 11, parameters->my_comm, &status);
			request_table[status.MPI_SOURCE].value = get_message[0];
			request_table[status.MPI_SOURCE].time = get_message[1];
			request_table[status.MPI_SOURCE].rank = status.MPI_SOURCE;
			printf("On %d Process %d DID send q request!\n", parameters->rank, index);
		}
	}
	set_lamport(get_message[1]);
}

/*3rd phase of comunication: send responses*/
void comm_3(basic_parameters *parameters, data *request_table) {
	int index, message[2];
	for (index = 0; index < parameters->hobos_count - 1; index++) {
		if (index < parameters->hobo_limit - 1) {
			message[0] = ACCESS_GRANTED;
			printf("ON: %d Grant access to %d LIMIT: %d\n", parameters->rank, request_table[index].rank, parameters->hobo_limit);
		} else if (index >= parameters->hobo_limit) {
			message[0] = ACCESS_DENIED;
			printf("ON: %d Denied access to %d\n", parameters->rank, request_table[index].rank);
		} else if (index == parameters->hobo_limit - 1) {
			printf("ON: %d Ignored for now. If I enter I will send to this thread ACCESS Denied %d\n", parameters->rank, request_table[index].rank);
			continue;
		}
		message[1] = get_lamport();
		MPI_Send(&message, 2, MPI_INT, request_table[index].rank, 2, parameters->my_comm);
	}
}

void serve_response(basic_parameters *parameters, int responses, int rival_rank) {
	int message[2];
	if (responses == ACCESS_GRANTED) {
		printf("ON: %d I will be in!\n", parameters->rank);
		message[0] = ACCESS_DENIED;
	} else {
		printf("ON: %d I will be out!\n", parameters->rank);
		message[0] = ACCESS_GRANTED;
	}
	message[1] = get_lamport();
	MPI_Send(&message, 2, MPI_INT, rival_rank, 2, parameters->my_comm);
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

int first_critical_section(basic_parameters *parameters, int *responses){
	int out_index;
	/*All receivers should save the same value, so I don't need array, except this I will use single variable*/
	data request_table[parameters->hobos_count];
	MPI_Request request_statuses[parameters->hobos_count];
	printf("First");
	comm_1_4(parameters, responses, request_statuses, request_table);
	printf("second__section\n");
	comm_2(parameters, request_table);
	printf("after 2nd\n");
	qsort(request_table, parameters->hobos_count - 1, sizeof(data), comparator);

	out_index = parameters->hobos_count;
	// while(out_index-- && parameters->rank==3){
	// 	printf(">>>>> INDEX: %d TIME: %d RANK:%d\n", out_index, request_table[out_index].time, request_table[out_index].rank );
	// }
	// sleep(100);
	//comm_3(parameters, request_table);
	int i_will_go = ACCESS_DENIED;
	while(out_index--){

		// if(request_statuses[out_index] != MPI_REQUEST_NULL){
		// 	MPI_Cancel(request_statuses + out_index);
		// }
		if (request_table[out_index].rank == parameters->rank){
			if( out_index < parameters->hobo_limit){
				i_will_go = ACCESS_GRANTED;
				printf("ON: %d I will be in!\n", parameters->rank);
			}else{
				printf("ON: %d I will be out!\n", parameters->rank);
			}
			break;
		}
	}
	/*Just wait for any response*/
	//MPI_Waitany(parameters->hobos_count, request_statuses, &out_index, MPI_STATUS_IGNORE);
	//printf("Response from: %d", out_index);
	
	//serve_response(parameters, responses[out_index * 2], request_table[parameters->hobo_limit - 1].rank);


	return i_will_go;
}

int second_critical_section(int rank, int *message){
	int nurses_ids[4] = {0,0,0,0};
	if(message[0]) {
		get_nurses(message[1], rank, nurses_ids);
		printf("Hobo: %d Status: I get enough help. I need release my nurses\n", rank);
		release_nurses(rank, nurses_ids);
	}
	printf("Hobo: %d Status: After 2nd critical section\n", rank);
}

int send_status_to_nurses(int hobos_count, int nurse_count, int *message){
	while (nurse_count--){
		message[2] = get_lamport();
		MPI_Send(message, 3, MPI_INT, hobos_count + nurse_count, 133, MPI_COMM_WORLD);
	}
}
int party(int rank, int *message, int status){
	if (status == ACCESS_GRANTED) {
		printf("ON: %d I am in!\n", rank);
		/*Randomly get drunk*/
		message[DRUNK] = rand() % 2;
		message[WEIGHT] = rand() % 4 + 1;
		printf("ON: %d status: %d and weight: %d\n", rank, message[DRUNK], message[WEIGHT]);
	} else {
		printf("ON: %d I am out!\n", rank);
	}
}

int release_nurses(int rank, int *nurses_ids){
	int index = 4;
	int message[2] = {I_AM_FINE, 0};
	while (index--) {
		if (nurses_ids[index]){
			message[1] = get_lamport();
			MPI_Send(&message, 2, MPI_INT, nurses_ids[index], 0, MPI_COMM_WORLD);
			printf("Hobo: %d relesed %d\n", rank, nurses_ids[index]);
		}
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

/*This function will be executed by Hobos processes*/
int hobo_live(basic_parameters *parameters) {
	int cycle_count = 0;
	while(cycle_count++ < parameters->loop_limit){
		int out_index, i_will_go;
		int message[3] = {0, 0, 0};
		int responses[parameters->hobos_count * 2];

		printf("========..%d..=======\n",cycle_count);

		i_will_go = first_critical_section(parameters, responses);

		printf("ON: %d at %d After 1st critical section\n", parameters->rank, lamport_value);

		party(parameters->rank, message, i_will_go);

		send_status_to_nurses(parameters->hobos_count, parameters->nurse_count, message);

		second_critical_section(parameters->rank, message);
		MPI_Barrier(MPI_COMM_WORLD);
	}
	printf("ON: %d END!\n", parameters->rank);
}

/*Recv from all hobos its statuses, return max lamport of all messages*/
int collect_patients(int patients_count, nurse_data *patients_register) {
	int lamport = 0;
	MPI_Status status;
	int recv_message[2] = {0, 0};
	while (patients_count--) {
		MPI_Recv(&recv_message, 3, MPI_INT, MPI_ANY_SOURCE, 133, MPI_COMM_WORLD, &status);
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
int help_patients(int rank, int patient_to_help) {
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
void nurse_job(basic_parameters *parameters, nurse_data *patients) {
	int offset = 0;
	int my_index, hobo_to_cure;
	int index = parameters->hobos_count;
	while (index--) {
		/*my index it's a relative index inside nurses group
		WARNING: my index will have value in range: <1..nurse_count>*/
		my_index = parameters->rank - parameters->hobos_count + 1 + offset * parameters->nurse_count;

		hobo_to_cure = patient_to_help(my_index, parameters->hobos_count, patients);
		
		if (hobo_to_cure > -1) {
			help_patients(parameters->rank, hobo_to_cure);
			offset++;
		} else {
			/*if current my_index isn't good enough there's no sense to looking for another hobo*/
			break;
		}
	}
}

/* Here all variables used by nurse process will be set to default values*/
void init_nurse_variables(nurse_data *patients, int index){
	while(index--){
		patients[index].if_drunk = 0;
		patients[index].weight = 0;
	}
}
/*This function will be executed by Nurses processes*/
int nurse_live(basic_parameters *parameters) {
	int cycle_count = 0;
	int lamport = 0;
	nurse_data patients[parameters->hobos_count];

	while(cycle_count++ < parameters->loop_limit){
		printf("NUR=====..%d..=======\n",cycle_count);

		printf("Nurse %d I wait for info from all\n", parameters->rank);
		init_nurse_variables(patients, parameters->hobos_count);
		lamport = collect_patients(parameters->hobos_count, patients);
		printf("Nurse %d I get info from all\n", parameters->rank);
		/*
		Now nurse know all about each hobo: what is its weight or if he is drunk
		So this is time to respond on help request: nurse needs to help hobo.
		Because each nurse has the same data, nurses will be NOT communicate with each other.
		They simply send a message to specific hobo. This will be mean, this nurse is helping this hobo.
		*/

		nurse_job(parameters, patients);

		MPI_Barrier(MPI_COMM_WORLD);
	}
	printf("Nurse: %d END\n", parameters->rank);
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
