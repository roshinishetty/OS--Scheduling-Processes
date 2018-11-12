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
	int last_start_time;
	char scheduling_policy;	//scheduling algo - fcfs (F) or round_robin (R)
	bool preemption;	//doesn't affect fcfs, required for multi for 1 case
	bool isCancelled;		//if process is canceled
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
ReadyQueue *readyQueue_fcfs;
ReadyQueue *readyQueue_rr;

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
	readyQueue_rr = (ReadyQueue*) malloc(sizeof(ReadyQueue));
	readyQueue_rr->capacity = COUNT;
	readyQueue_rr->harr = (int*) malloc(sizeof(int)*COUNT);
	readyQueue_rr->heap_size = 0;

	readyQueue_fcfs = (ReadyQueue*)malloc(sizeof(ReadyQueue));
	readyQueue_fcfs->capacity = COUNT;
	readyQueue_fcfs->harr = (int*)malloc(sizeof(int)*COUNT);
	readyQueue_fcfs->heap_size = 0;
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

void createTimerExpiredEvent(Process* p) {
	Event* e = (Event *) malloc(sizeof(Event));
	e->type = TimerExpired;
	e->time = p->start_time + 4;
	e->pid = p->pid;
	e->isCancelled = 'F';
	insertEvent(e);
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

void cancelCPUBurst(int p) {
	for(int i=0;i<eventHeap->heap_size; i++) {
		Event* ev = eventHeap->harr[i];
		if( ev->pid == p && ev->type == CPUburstCompletion)
			ev->isCancelled == 'T';
	}
}

void cancelTimerExpiration(int p) {
	for(int i=0;i<eventHeap->heap_size; i++) {
		Event* ev = eventHeap->harr[i];
		if( ev->pid == p && ev->type == TimerExpired)
			ev->isCancelled == 'T';
	}
}

Event* getEvent () {
	if(eventHeap->heap_size <= 0) 
		return NULL;
	Event* root = eventHeap->harr[0];
	if(eventHeap->heap_size == 1) {
		eventHeap->heap_size--;
	} else {
		eventHeap->harr[0] = eventHeap->harr[eventHeap->heap_size-1];
		eventHeap->heap_size--;
		MinHeapify(0); 
	}
		return root;
}

Process *createProcess(int sno) {
	Process* p = (Process*) malloc(sizeof(Process));
	p->pid = processes[sno][0];
	p->arrival_time = processes[sno][1];
	p->cpu_burst = processes[sno][2];
	p->wait_time = 0;
	p->last_start_time = -1;
	if(p->cpu_burst<=8) {
		p->scheduling_policy = 'R';
		p->preemption = 'T';
	} else {
		p->scheduling_policy = 'F';
		p->preemption = 'F';
	}
	p->isCancelled ='F';
	printf("Process %d created\n", p->pid);
	return p;
}

void addToReadyQueue1(Process* p) {
	if (readyQueue_rr->heap_size == readyQueue_rr->capacity) {
		printf("Overflow in ready queue\n");
		return;
	}
	readyQueue_rr->heap_size++;
	int i = readyQueue_rr->heap_size-1;
	readyQueue_rr->harr[i] = p->pid;

	while(i != 0) {
		int curr_pid = readyQueue_rr->harr[i];
		int parent_pid = readyQueue_rr->harr[parent(i)];

		int curr_last_start = ProcessTable[curr_pid]->last_start_time;
		int parent_last_start = ProcessTable[parent_pid]->last_start_time;
		
		int curr_arrival = ProcessTable[curr_pid]->arrival_time;
		int parent_arrival = ProcessTable[parent_pid]->arrival_time;
		
		if(curr_last_start < parent_last_start || (curr_last_start == parent_last_start && curr_arrival < parent_arrival)) {
			int temp = readyQueue_rr->harr[i];
			readyQueue_rr->harr[i] = readyQueue_rr->harr[parent(i)];
			readyQueue_rr->harr[parent(i)] = temp;			
		}
		
		else
			break;

		i=parent(i);
 	}
}

void addToReadyQueue2(Process* p) {
	if (readyQueue_fcfs->heap_size == readyQueue_fcfs->capacity) {
		printf("Overflow in ready queue\n");
		return;
	}
	readyQueue_fcfs->heap_size++;
	int i =readyQueue_fcfs->heap_size-1;
	readyQueue_fcfs->harr[i] = p->pid;

	while(i!=0) {
		int curr_pid = readyQueue_fcfs->harr[i];
		int parent_pid = readyQueue_fcfs->harr[parent(i)];
		int curr_arrival = ProcessTable[curr_pid]->arrival_time;
		int parent_arrival = ProcessTable[parent_pid]->arrival_time;   
		
		if(curr_arrival < parent_arrival || (curr_arrival == parent_arrival && curr_pid < parent_pid)) {
		int temp = readyQueue_fcfs->harr[i];
		readyQueue_fcfs->harr[i] = readyQueue_fcfs->harr[parent(i)];
		readyQueue_fcfs->harr[parent(i)] = temp;

		i = parent(i);
		} else {
			break;
		}
	}
}

void MinHeapifyReadyQueue1(int i) {
	int l = left(i);
	int r = right(i);

	int smallest = i;
	if(l < readyQueue_rr->heap_size) {

		int curr_pid = readyQueue_rr->harr[i];
		Process* curr_process = ProcessTable[curr_pid];
		int curr_last_start = curr_process->last_start_time;

		int left_pid = readyQueue_rr->harr[l];
		Process* left_process = ProcessTable[left_pid];
		int left_last_start = left_process->last_start_time;
	
		if(left_last_start < curr_last_start ||
			(left_last_start == curr_last_start && left_process->arrival_time < curr_process->arrival_time))
			smallest = l;
	}
	if( r < readyQueue_rr->heap_size) {
		int right_pid = readyQueue_rr->harr[r];
		Process* right_process = ProcessTable[right_pid];
		int right_last_start = right_process->last_start_time;
		int right_arrival = right_process->arrival_time;

		int smallest_pid = readyQueue_rr->harr[smallest];
		Process* smallest_process = ProcessTable[smallest_pid];
		int smallest_last_start = smallest_process->last_start_time;
		int smallest_arrival = smallest_process->arrival_time;	

		if(right_last_start < smallest_last_start ||
			(right_last_start == smallest_last_start && right_arrival < smallest_arrival))
			smallest =r;	
	}

	if(smallest != i ) {
		int temp = readyQueue_rr->harr[i];
		readyQueue_rr->harr[i] = readyQueue_rr->harr[smallest];
		readyQueue_rr->harr[smallest] = temp;
		MinHeapifyReadyQueue1(smallest);
	}

}

void MinHeapifyReadyQueue2(int i) {
	int l = left(i);
	int r = right(i);

	int smallest = i;
	if( l < readyQueue_fcfs->heap_size) {

		int curr_pid = readyQueue_fcfs->harr[i];
		int curr_arrival = ProcessTable[curr_pid]->arrival_time;

		int left_pid = readyQueue_fcfs->harr[l];
		int left_arrival = ProcessTable[left_pid]->arrival_time;
	
		if(left_arrival < curr_arrival || (left_arrival==curr_arrival && left_pid < curr_pid))
			smallest = l;
	}
	
	if( r < readyQueue_fcfs->heap_size) {
		int right_pid = readyQueue_fcfs->harr[r];
		int right_arrival = ProcessTable[right_pid]->arrival_time;
	
		int smallest_pid = readyQueue_fcfs->harr[smallest];
		int smallest_arrival = ProcessTable[smallest_pid]->arrival_time;
		
		if(right_arrival < smallest_arrival || (right_arrival==smallest_arrival && right_pid < smallest_pid))
			smallest = r;
	}	

	if(smallest != i ) {
		int temp = readyQueue_fcfs->harr[i];
		readyQueue_fcfs->harr[i] = readyQueue_fcfs->harr[smallest];
		readyQueue_fcfs->harr[smallest] = temp;
		MinHeapifyReadyQueue2(smallest);
	}
}

Process* moveFromReadyQueue1() {
	if(readyQueue_rr->heap_size <= 0) {
		return NULL;
	} 
	if(readyQueue_rr->heap_size == 1) {
		readyQueue_rr->heap_size--;
		return ProcessTable[readyQueue_rr->harr[0]];
	}	

	int root = readyQueue_rr->harr[0];
	readyQueue_rr->harr[0] = readyQueue_rr->harr[readyQueue_rr->heap_size-1];
	readyQueue_rr->heap_size--;
	MinHeapifyReadyQueue1(0);

	return ProcessTable[root];	
}
	
Process* moveFromReadyQueue2() {
	if(readyQueue_fcfs->heap_size <= 0) {
		return NULL;
	} 
	if(readyQueue_fcfs->heap_size == 1) {
		readyQueue_fcfs->heap_size--;
		return ProcessTable[readyQueue_fcfs->harr[0]];
	}	

	int root = readyQueue_fcfs->harr[0];
	readyQueue_fcfs->harr[0] = readyQueue_fcfs->harr[readyQueue_fcfs->heap_size-1];
	readyQueue_fcfs->heap_size--;
	MinHeapifyReadyQueue2(0);

	return ProcessTable[root];
}

int count_of_processes_completed = 0;
void EventSimulation() {
	initReadyQueue();
	initEventHeap(); 

	while(eventHeap->heap_size>0) {
		/*for(int i=0;i<eventHeap->heap_size;i++)
			printf("%d\t", eventHeap->harr[i]->pid);
		printf("\n");*/
		Event* curr_event = getEvent();
		
		time = curr_event->time;

		switch(curr_event->type) {
			case Arrival:
			{
				Process* p= createProcess(curr_event->sno);
				ProcessTable[curr_event->pid] = p;  
				
				if(CPU == -1) {
					printf("Process currently running at time %d - %d\n",time, p->pid);
					p->state = 'U';
					CPU = p->pid;
					p->start_time = time;
					p->wait_time += time - p->arrival_time;
					if(p->scheduling_policy=='R' && p->cpu_burst > 4)
						createTimerExpiredEvent(p);
					else
						createCPUBurstEvent(p);
				} else {
					printf("Process %d added to the ready queue\n", p->pid);
					p->state = 'E';
					//printf("Process %d added to the ready queue\n", p->pid);
					if(p->scheduling_policy=='R')
						addToReadyQueue1(p);
					else if(p->scheduling_policy == 'F')
						addToReadyQueue2(p);
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
				printf("CPUburstCompletion of process at time %d - %d\n",time, curr_event->pid);
				ProcessTable[curr_event->pid]->state = 'T';

				count_of_processes_completed++;
				if(readyQueue_rr->heap_size == 0 && readyQueue_fcfs->heap_size == 0) {
					printf("No process in readyQueue\n");
					return;
				} else {
					Process* p;
					if(readyQueue_rr->heap_size>0)
						p = moveFromReadyQueue1();
					else 
						p = moveFromReadyQueue2();
					printf("Process selected from ready queue - %d\n", p->pid);
					printf("Process currently running at time %d - %d\n",time, p->pid);
					CPU = p->pid;
					p->state = 'U';
					p->start_time = time;
					if(p->last_start_time!=-1)
						p->wait_time += time - p->last_start_time-4;
					else
						p->wait_time += time - p->arrival_time;

					if(p->scheduling_policy=='R' && p->cpu_burst > 4)
						createTimerExpiredEvent(p);
					else
						createCPUBurstEvent(p);
				}
				break;
			}
			case TimerExpired:	
			{
				printf("Timer expired at time %d", time);
				Process* p = ProcessTable[curr_event->pid];
				p->cpu_burst -= 4;			
				p->last_start_time = p->start_time;
				
				addToReadyQueue1(p);

				Process* next_process = moveFromReadyQueue1();
				printf("Process selected from ready queue - %d\n", next_process->pid);
				printf("Process currently running at time %d - %d\n",time, next_process->pid);
				
				CPU = next_process->pid;
				next_process->state = 'U';
				next_process->start_time = time;
				if(next_process->last_start_time!=-1)
					next_process->wait_time += time - next_process->last_start_time-4;
				else
					next_process->wait_time += time - p->arrival_time;

				if(p->scheduling_policy=='R' && p->cpu_burst > 4)
					createTimerExpiredEvent(p);
				else
					createCPUBurstEvent(p);
				
				break;
			}
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