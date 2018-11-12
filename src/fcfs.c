#include<stdio.h>
#include<stdbool.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>
#define COUNT 20

typedef struct Process {
	int pid;			//process ID
	char state;			//state- running(U), ready(E), terminated(T)
	int arrival_time;	//arrival time from csv file
	int cpu_burst;		//cpu burst time from csv file
	int wait_time;		//CPU waiting time, calculated for each process
	int start_time;		//when the process started execution
//	char* scheduling_policy;	//scheduling algo - fcfs or round_robin
//	int time_quantum;	//time quantum =0 if fcfs, else 8 if round_robin
	bool preemption;	//doesn't affect fcfs, required for multi for 1 case
	bool isCanceled;		//if process is canceled
} Process;				//structure of process

Process* ProcessTable[COUNT]; 	// an array of processes via gant chart
int CPU=-1;	// contains pid of current working process, -1 if idle
int SNO=0;
int time=0;
int processes[COUNT][3]; //a table containing all processes

typedef struct Event {
	enum eventType{Arrival, CPUburstCompletion, TimerExpired} type;
	double time;
	int pid;
	int sno;
	bool isCancelled;
} Event;

typedef struct EventQueue {
	Event *harr[COUNT];
	int capacity;
	int heap_size;
} EventQueue;

typedef struct ReadyQueue {
	int *harr;
	int capacity;
	int heap_size; 
} ReadyQueue;

EventQueue *eventHeap;
ReadyQueue *readyQueue;

void loadTable() {
	FILE* fptr;
	fptr = fopen("process.csv","r");
	if(!fptr) {
		fprintf(stderr, "Can't open file.\n");
		return;
	}
	char buf[1024];
	for(int i=0;i<COUNT;i++) {
		fgets(buf, sizeof(buf), fptr);
		processes[i][0]=i;
		int j=0; //j stores the number after converting from
		int k=0; //k for the character number in the string buffer
		while(buf[k]!=',') {
			j= j*10+(buf[k]-48);
			k++;
		}	
		k++;
		processes[i][1]=j;
		j=0;
		while(buf[k]!='\n'){
			j= j*10+(buf[k]-48);
			k++;
		}
		processes[i][2]=j;
	}		
	fclose(fptr);

	for(int i=0;i<COUNT;i++) {
		printf("%d %d %d\n", processes[i][0], processes[i][1], processes[i][2]);
	}
	return ;
}

void swap_processes(int x, int y) {
	int a=processes[x][0];
	processes[x][0]=processes[y][0];
	processes[y][0]=a;

	a=processes[x][1];
	processes[x][1]=processes[y][1];
	processes[y][1]=a;

	a=processes[x][2];
	processes[x][2]=processes[y][2];
	processes[y][2]=a;		
}

void sortProcess () {
	for(int j=1;j<COUNT;j++) {
		for(int i=0;i<COUNT-j;i++) {
			if(processes[i][1]>processes[i+1][1] ) {
				swap_processes(i,i+1);
			}
		}
	}	
}

int parent(int i) {
	return (i-1)/2;
}

int left(int i) {
	return (2*i+1);
}

int right(int i) {
	return (2*i+2);
}

void initReadyQueue() {
	readyQueue = (ReadyQueue*)malloc(sizeof(ReadyQueue));
	readyQueue->capacity = COUNT;
	readyQueue->harr = (int*)malloc(sizeof(int)*COUNT);
	readyQueue->heap_size = 0;
}

void insertEvent(Event* e) {
	if(eventHeap->heap_size == eventHeap->capacity) {
		printf("Overflow in event heap\n");
		return;
	}
	eventHeap->heap_size++;
	int i= eventHeap->heap_size -1;
	eventHeap->harr[i] = e;

	while(i!=0 && eventHeap->harr[parent(i)]->time > eventHeap->harr[i]->time) {
		Event* e = eventHeap->harr[i];
		eventHeap->harr[i] = eventHeap->harr[parent(i)];
		eventHeap->harr[parent(i)] = e;	
		i = parent(i);
	}
}

void createArrivalEvent() {
	Event* e=(Event *)malloc(sizeof(Event));
	e->type=Arrival;
	e->time=processes[SNO][1];
	e->pid=processes[SNO][0];
	e->isCancelled='F';
	e->sno = SNO;
	insertEvent(e);
}

void createCPUBurstEvent(Process* p) {
	Event* ev = (Event*) malloc(sizeof(Event));
	ev->type = CPUburstCompletion;
	ev->time = p->start_time + p->cpu_burst;
	ev->pid = p->pid;
	ev->isCancelled = 'F';
	insertEvent(ev);
}

void initEventHeap() {
	eventHeap = (EventQueue*) malloc(sizeof(EventQueue));
	eventHeap->heap_size=0;
	eventHeap->capacity=COUNT;

	createArrivalEvent();
	SNO++;
}

void MinHeapify(int i) {
	int l = left(i);
	int r = right(i);
	int smallest = i;
	if(l < eventHeap->heap_size && eventHeap->harr[l]->time < eventHeap->harr[i]->time) 
		smallest = l;
	if(r < eventHeap->heap_size && eventHeap->harr[r]->time < eventHeap->harr[smallest]->time) 
		smallest = r;	
	if(smallest != i) {
		Event* e = eventHeap->harr[i];
		eventHeap->harr[i] = eventHeap->harr[smallest];
		eventHeap->harr[smallest] = e;	
		MinHeapify(smallest);
	}
} 

Event* getEvent () {
	if(eventHeap->heap_size <= 0) 
		return NULL;
	if(eventHeap->heap_size == 1) {
		eventHeap->heap_size--;
		return eventHeap->harr[0];
	}
	Event* root = eventHeap->harr[0];
	eventHeap->harr[0] = eventHeap->harr[eventHeap->heap_size-1];
	eventHeap->heap_size--;
	MinHeapify(0);
	return root;
}

Process *createProcess(int time, int sno) {
	Process* p = (Process*) malloc(sizeof(Process));
	p->pid = processes[sno][0];
	p->arrival_time = processes[sno][1];
	p->cpu_burst = processes[sno][2];
	p->wait_time = 0;
	p->preemption = 'F';
	p->isCanceled ='F';
	return p;
}

void addToReadyQueue(Process* p) {
	if (readyQueue->heap_size == readyQueue->capacity) {
		printf("Overflow in ready queue\n");
		return;
	}
	readyQueue->heap_size++;
	int i =readyQueue->heap_size-1;
	readyQueue->harr[i] = p->pid;

	while(i!=0) {
		int curr_pid = readyQueue->harr[i];
		int parent_pid = readyQueue->harr[parent(i)];
		int curr_arrival = ProcessTable[curr_pid]->arrival_time;
		int parent_arrival = ProcessTable[parent_pid]->arrival_time;   
		
		if(curr_arrival < parent_arrival || (curr_arrival == parent_arrival && curr_pid < parent_pid)) {
		int temp = readyQueue->harr[i];
		readyQueue->harr[i] = readyQueue->harr[parent(i)];
		readyQueue->harr[parent(i)] = temp;

		i = parent(i);
		} else {
			break;
		}
	}
}

void MinHeapifyReadyQueue(int i) {
	int l = left(i);
	int r = right(i);

	int curr_pid = readyQueue->harr[i];
	int curr_arrival = ProcessTable[curr_pid]->arrival_time;

	int smallest = i;
	if( l < readyQueue->heap_size) {
		int left_pid = readyQueue->harr[l];
		int left_arrival = ProcessTable[left_pid]->arrival_time;
	
		if(left_arrival < curr_arrival || (left_arrival==curr_arrival && left_pid < curr_pid))
			smallest = l;
	}
	
	if( r < readyQueue->heap_size) {
		int right_pid = readyQueue->harr[r];
		int right_arrival = ProcessTable[right_pid]->arrival_time;
	
		int smallest_pid = readyQueue->harr[smallest];
		int smallest_arrival = ProcessTable[smallest_pid]->arrival_time;
		if(right_arrival < smallest_arrival || (right_arrival==smallest_arrival && right_pid < smallest_pid))
			smallest = r;
	}	

	if(smallest != i ) {
		int temp = readyQueue->harr[i];
		readyQueue->harr[i] = readyQueue->harr[smallest];
		readyQueue->harr[smallest] = temp;
		MinHeapifyReadyQueue(smallest);
	}
}
	
Process* moveFromReadyQueue() {
	if(readyQueue->heap_size <= 0) {
		return NULL;
	} 
	if(readyQueue->heap_size == 1) {
		readyQueue->heap_size--;
		return ProcessTable[readyQueue->harr[0]];
	}	

	int root = readyQueue->harr[0];
	readyQueue->harr[0] = readyQueue->harr[readyQueue->heap_size-1];
	readyQueue->heap_size--;
	MinHeapifyReadyQueue(0);

	return ProcessTable[root];
}

int count_of_processes_completed = 0;
void EventSimulation() {
	initReadyQueue();
	initEventHeap(); 

	while(eventHeap->heap_size>0) {
		Event* curr_event = getEvent();
		time = curr_event->time;

		switch(curr_event->type) {
			case Arrival:
			{
				Process* p= createProcess(time,curr_event->sno);
				ProcessTable[curr_event->pid] = p;  
	
				if(CPU == -1) {
					printf("Process currently running at time %d- %d\n", time, p->pid);
					p->state = 'U';
					CPU = p->pid;
					p->start_time = time;
					p->wait_time += time - p->arrival_time;
					createCPUBurstEvent(p);
				} else {
					p->state = 'E';
					printf("Process %d added to the ready queue\n", p->pid);
					addToReadyQueue(p);
				}
				if(SNO<COUNT) {
				//	printf("Arrival event creation\n");
					createArrivalEvent();
					SNO++;			
				}
				break;
			}
			case CPUburstCompletion:
			{
				printf("CPUburstCompletion of process at time %d- %d\n", time, curr_event->pid);
				ProcessTable[curr_event->pid]->state = 'T';
				count_of_processes_completed++;
				if(readyQueue->heap_size == 0) {
					printf("No process in readyQueue\n");
					return;
				} else {
					Process* p = moveFromReadyQueue();
					printf("Process selected from ready queue - %d\n", p->pid);
					printf("Process currently running at time %d- %d\n", time, p->pid);
					CPU = p->pid;
					p->state = 'U';
					p->start_time = time;
					p->wait_time = time - p->arrival_time;
					createCPUBurstEvent(p);
				}
				break;
			}
			//case TimerExpired:	{}
			default:
			{
				printf("default case\n");
			}			
		}
	}
}

int calculateAWT () {
	int AWT=0;
	for(int i =0;i<COUNT;i++) {
		Process *p=ProcessTable[i];
		AWT += p->wait_time;
	}	
	return AWT/COUNT;
}

int main() {
	loadTable();
	sortProcess();
	EventSimulation();
	printf("Average waiting time = %d\nprocesses completed=%d\n",calculateAWT(), count_of_processes_completed);
	return 0;
}