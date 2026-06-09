#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>      //for fork, sleep, usleep, daemon, kill
#include <signal.h>      //for SIGSTOP, SIGCONT, SIGTERM
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>     //for waitpid

#define MAX_PROCS 1000

struct Process {
    pid_t pid;
    char cmd[512];
    int submitted;
    int queued;
    int completed;
    int completion_time;
    int wait_time;
};

struct ProcTable {
    struct Process processArray[MAX_PROCS];
    int count;
    sem_t mutex;
};

struct ProcTable *procTable;
int shm_fd;
int NCPU;
int TSLICE;
bool running = true;     //flag for scheduler loop

void handle_sigterm(int sig) { running = false; }  //when ctrl c pressed, shell sends SIGTERM to scheduler, this sets running = false

struct Queue {
    int front, rear, size, capacity;
    struct Process *array[MAX_PROCS];
};

void initQueue(struct Queue *q) {   //initiliaze queue
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    q->capacity = MAX_PROCS;
}

bool isEmpty(struct Queue *q) { return q->size == 0; }

void enqueue(struct Queue *q, struct Process *p) {
    if (q->size >= q->capacity) return;
    q->rear = (q->rear + 1) % q->capacity;
    q->array[q->rear] = p;
    q->size++;
}

struct Process *dequeue(struct Queue *q) {
    if (isEmpty(q)) return NULL;
    struct Process *p = q->array[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return p;
}

void schedule_RR() {      // main scheduler loop  - each iteration means 1 Tslice
    struct Queue ready, runningQ;
    initQueue(&ready);
    initQueue(&runningQ);

    while (running) {
        usleep(TSLICE * 1000);         //The scheduler wakes up every TSLICE, performs scheduling and then sleeps again

        if (sem_wait(&procTable->mutex) == -1) {     //lock shared memory
            perror("sem_wait");
            exit(1);
        }

        for (int i = 0; i < procTable->count; i++) {           //move new jobs in ready queue
            struct Process *p = &procTable->processArray[i];
            if (p->submitted && !p->completed && !p->queued) {
                enqueue(&ready, p);
                p->queued = 1;
            }
        }


        //Sends SIGSTOP to all running processes and places them back into the ready queue to resume next round
        while (!isEmpty(&runningQ)) {
            struct Process *p = dequeue(&runningQ);
            if (!p->completed) {
                if (kill(p->pid, SIGSTOP) == -1)
                    perror("SIGSTOP");
                enqueue(&ready, p);
            }
        }



        //start running upto NCPU jobs
        int started = 0;
        while (!isEmpty(&ready) && started < NCPU) {
            struct Process *p = dequeue(&ready);
            int status;
            pid_t result = waitpid(p->pid, &status, WNOHANG);
            if (result > 0) {            // If a process finishes (waitpid returns > 0) mark as completed.
                p->completed = 1;
                continue;
            }

            if (!p->completed) {
                if (kill(p->pid, SIGCONT) == -1) {  //SIGCONT used as signal to start process
                    perror("SIGCONT");
                    p->completed = 1;
                    continue;
                }
                p->queued = 0;             //If process not completed, put in running queue
                enqueue(&runningQ, p);
            }
            started++;
        }

        for (int i = 0; i < procTable->count; i++) {              //update completion and wait time
            struct Process *p = &procTable->processArray[i];
            if (p->submitted && !p->completed) {
                p->completion_time++;
                if (p->queued)
                    p->wait_time++;
            }
        }



        //if all jobs done and queues empty, release semaphore, sleep till shell termination signal
        bool all_done = true;
        for (int i = 0; i < procTable->count; i++) {
            if (!procTable->processArray[i].completed) {
                all_done = false;
                break;
            }
        }

        if (all_done && isEmpty(&ready) && isEmpty(&runningQ)) {
            sem_post(&procTable->mutex);
            usleep(TSLICE * 1000);
            continue;
        }

        if (sem_post(&procTable->mutex) == -1)
            perror("sem_post");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Format: %s NCPU TSLICE\n", argv[0]);
        exit(1);
    }

    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    if (NCPU <= 0 || TSLICE <= 0) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(1);
    }

    if (daemon(0, 0) == -1) {
        perror("daemon");
        exit(1);
    }

    shm_fd = shm_open("/simple_shm", O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    procTable = mmap(NULL, sizeof(struct ProcTable), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (procTable == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    signal(SIGTERM, handle_sigterm);
    schedule_RR();

    munmap(procTable, sizeof(struct ProcTable));
    close(shm_fd);
    return 0;
}