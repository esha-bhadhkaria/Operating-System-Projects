#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      //for fork, execvp, getpid, kill
#include <signal.h>
#include <time.h>        //for SIGINT (ctrl c), SIGCHLD
#include <sys/wait.h>  
#include <sys/mman.h>    //all these for shared memory - mmap and shm_open
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>   //to sync shell and schedular
#include <errno.h>

#define LINE_LIM 512
#define ARG_LIM 100
#define PROCESSES_LIM 1000     //max number of processes entered in shell

struct Process {
    pid_t pid;              //PID of process after fork
    char cmd[LINE_LIM];     //command input by user
    //flags for jobs
    int submitted;
    int queued;
    int completed;

    int completion_time;     //total execution time in multiples of t slice
    int wait_time;           //total wait time in multiples of t slice
};

//This Process table (shared memory struct holding all processes info) is shared by Shell and Scheduler
struct ProcTable { 
    struct Process processArray[PROCESSES_LIM];    //array of all processes submitted by user
    int count;               //no. of jobs in table 
    sem_t mutex;             //semaphore to sync shell and schdeuler (to avoid race)
};

struct ProcTable *procTable;
int shm_fd;
int schedulerPID;
pid_t mainPID;   //PID of shell
char *NCPU;
char *TSLICE;

char *trim(char *A) {
    while (*A == ' ' || *A == '\t') A++;
    char *end = A + strlen(A) - 1;
    while (end > A && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return A;
}

int parse_args(char *cmd, char **argv) {
    int argc = 0;
    char *token = strtok(cmd, " \t");
    while (token != NULL && argc < ARG_LIM - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

//ctrl c handler
void handle_sigint(int sig) {
    if (mainPID == getpid()) {
        if (sem_wait(&procTable->mutex) == -1) {
            perror("sem_wait");
            exit(1);
        }

        printf("\nPID      COMMAND                   COMPLETION TIME(in Tslice multiples) WAIT TIME(in Tslice multiples)\n");
        for (int i = 0; i < procTable->count; i++) {
            struct Process *p = &procTable->processArray[i];
            if (p->submitted) {
                int completion = p->completion_time <= 0 ? 1 : p->completion_time;
                printf("%-15d %-25s %-15d %-15d\n", p->pid, p->cmd, completion, p->wait_time);
            }
        }

        if (sem_post(&procTable->mutex) == -1)
            perror("sem_post");

        kill(schedulerPID, SIGTERM);                  //send signal to schedular to terminate

        //cleanup
        sem_destroy(&procTable->mutex);
        munmap(procTable, sizeof(struct ProcTable));
        close(shm_fd);
        shm_unlink("/simple_shm");
        exit(0);
    }
}

//SIGCHLD Handler
void handle_sigchld(int sigID, siginfo_t *info, void *whatever) {
    if (sigID == SIGCHLD) {
        pid_t sender = info->si_pid;
        if (sender != schedulerPID) {
            if (sem_wait(&procTable->mutex) == -1) {
                perror("sem_wait");
                exit(1);
            }

            for (int i = 0; i < procTable->count; i++) {
                if (procTable->processArray[i].pid == sender) {
                    procTable->processArray[i].completed = 1;         //Mark process as completed in table when it terminates. 
                    procTable->processArray[i].queued = 0;
                    break;
                }
            }

            if (sem_post(&procTable->mutex) == -1)
                perror("sem_post");
        }
    }
}

int processSubmit(char *command) {           //remove submit from submit ./job1
    char *space = strchr(command, ' ');
    if (space != NULL)
        memmove(command, space+1, strlen(command));

    char *argv[ARG_LIM];
    int argc = parse_args(command, argv);

    if (argc != 1) {
        fprintf(stderr, "Error: program cannot take extra command line arguments\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        if (kill(pid, SIGSTOP) == -1)       //Parent immediately stops the child, so it doesn't run immediately (wait for signal from schedualr)
            perror("SIGSTOP");
        return pid;
    }
}

void launch_command(char *input) {
    if (strncmp(input, "submit", 6) == 0) {
        if (sem_wait(&procTable->mutex) == -1) {
            perror("sem_wait");
            exit(1);
        }

        int idx = procTable->count;
        int pid = processSubmit(input);
        if (pid == -1) {
            sem_post(&procTable->mutex);
            return;
        }

        struct Process *p = &procTable->processArray[idx];       //intiliaze new job entry
        p->pid = pid;
        strcpy(p->cmd, input);
        p->submitted = 1;
        p->queued = 0;
        p->completed = 0;
        p->completion_time = 0;
        p->wait_time = 0;
        procTable->count++;

        if (sem_post(&procTable->mutex) == -1)
            perror("sem_post");

        return;
    }

    fprintf(stderr, "Invalid command.\n");
}

void main_loop() {
    char line[LINE_LIM];
    while (1) {
        printf("SimpleShell$ ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        line[strcspn(line, "\n")] = '\0';
        char *input = trim(line);
        if (strlen(input) == 0)
            continue;
        launch_command(input);
    }
}

int main(int argc, char **argv) {
    mainPID = getpid();

    if (argc != 3) {
        fprintf(stderr, "Format: %s NCPU TSLICE\n", argv[0]);
        exit(1);
    }

    NCPU = argv[1];
    TSLICE = argv[2];

    if (atoi(NCPU) <= 0 || atoi(TSLICE) <= 0) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(1);
    }

    shm_fd = shm_open("/simple_shm", O_CREAT | O_RDWR, 0666);      //create shared memory
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(struct ProcTable)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    procTable = mmap(NULL, sizeof(struct ProcTable), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (procTable == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    procTable->count = 0;
    if (sem_init(&procTable->mutex, 1, 1) == -1) {
        perror("sem_init");
        exit(1);
    }


    //setting up SIGINT AND SIGCHLD signal handlers
    struct sigaction sa_int, sa_chld;
    memset(&sa_int, 0, sizeof(sa_int));
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_int.sa_handler = handle_sigint;
    sa_chld.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_RESTART;
    sa_chld.sa_sigaction = handle_sigchld;
    sigaction(SIGINT, &sa_int, NULL);
    sigaction(SIGCHLD, &sa_chld, NULL);

    pid_t pid = fork();         //fork and launch scheduler
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        execl("./SimpleScheduler", "SimpleScheduler", NCPU, TSLICE, NULL);
        perror("execl");
        exit(1);
    } else {
        schedulerPID = pid;
    }

    main_loop();       //run shell loop and cleanup

    sem_destroy(&procTable->mutex);
    munmap(procTable, sizeof(struct ProcTable));
    close(shm_fd);
    shm_unlink("/simple_shm");

    return 0;
}