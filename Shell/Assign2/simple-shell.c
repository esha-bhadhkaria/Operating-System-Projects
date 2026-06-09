#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

#define LINE_LIM 512
#define ARG_LIM 100       
#define HIS_LIM 100     
#define PIPE_LIM 50 

//Structure for history
struct Entry_in_History {
    char command[LINE_LIM]; //command string entered by the user
    pid_t pid;              //PID of the last process that executed the command
    time_t start_time;
    double duration;        
};

struct Entry_in_History history[HIS_LIM];
int history_count = 0;

volatile sig_atomic_t sigint_received = 0; // Flag to indicate Ctrl-C (SIGINT) was received

// Removing extra whitespaces
char *trim(char *A) {
    while (*A == ' ' || *A == '\t') A++;
    char *end= A +strlen(A)-1;
    while (end > A && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return A;
}

//Parse command into argument array for execvp
int parse_args(char *cmd, char **argv) {
    int argc = 0;
    char *PART = strtok(cmd, " \t");  // split by space

    while (PART != NULL && argc < ARG_LIM-1) {
        argv[argc++] = PART;
        PART = strtok(NULL, " \t");  
    }

    argv[argc] = NULL; 
    return argc;                    
}

//Parse input line into pipeline segments separated by |
int parse_pipeline(char *line, char **segments) {
    int count = 0;
    char *token = strtok(line, "|");  // split by |

    while (token != NULL && count < PIPE_LIM) {
        segments[count++] = trim(token);
        token = strtok(NULL, "|");
    }

    return count;
}

//Add a command execution record to history
void add_history(const char *cmd, pid_t pid, time_t start, double duration) {
    if (history_count < HIS_LIM) {
        // Store command, PID, start time and duration in history array
        strncpy(history[history_count].command, cmd, LINE_LIM - 1);
        history[history_count].command[LINE_LIM - 1] = '\0';
        history[history_count].pid = pid;
        history[history_count].start_time = start;
        history[history_count].duration = duration;
        history_count++;
    } else {
        fprintf(stderr, "History is full, cannot store more commands.\n");
    }
}

//Print the list of commands executed
void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i].command);
    }
}

// Termination report
void print_report() {
    printf("\nTermination Report\n\n");
    printf("%-3s %-25s %-7s %-20s %-10s\n",
           " ", "COMMAND", "PID", "START TIME", "DURATION");
    

    for (int i = 0; i < history_count; i++) {
        char *time_str = ctime(&history[i].start_time);

        if (time_str) {
            time_str[strcspn(time_str, "\n")] = '\0';
        }

        printf("%-3d %-25s %-7d %-20s %.6f s\n",
               i + 1,
               history[i].command,
               history[i].pid,
               time_str ? time_str : "ERROR",
               history[i].duration);
    }

    printf("\nEND OF THE REPORT.\n");
}

//Handling Ctrl-C
void handle_sigint(int sig) {
    (void)sig;
    sigint_received = 1; 
}

//Execute a pipeline of commands
void launch(char **segments, int n, const char *full_cmd) {
    int pipes[PIPE_LIM - 1][2];  // Array of pipe descriptors
    pid_t pids[PIPE_LIM];       // Array to hold child process PIDs

    // Create pipes
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
    }

    struct timespec start, end;

    // Start measuring execution time
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime failed");
        start.tv_sec = 0;
        start.tv_nsec = 0;
    }

    // Fork child processes for each pipeline segment
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0) {  // Child process
            if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);    // Previous output → stdin
            if (i < n - 1) dup2(pipes[i][1], STDOUT_FILENO); // stdout → Next input

            // Close unused pipe ends
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char *argv[ARG_LIM];
            parse_args(segments[i], argv);
            execvp(argv[0], argv);       
            perror("execvp failed"); 
            exit(1);
        } else {
            pids[i] = pid;  // Store child PID in array
        }
    }

    // Parent closes all pipe ends
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children to finish
    for (int i = 0; i < n; i++) {
        if (waitpid(pids[i], NULL, 0) == -1) {
            perror("waitpid failed");
        }
    }

    // Measure end time
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime failed");
        end.tv_sec = start.tv_sec;
        end.tv_nsec = start.tv_nsec;
    }

    // Calculate duration
    double duration = (end.tv_sec - start.tv_sec) +
                      (end.tv_nsec - start.tv_nsec) / 1e9;

    // Add command to history
    add_history(full_cmd, pids[n - 1], time(NULL), duration);
}

//shell loop
int main() {
    signal(SIGINT, handle_sigint);  // Set up Ctrl-C handler

    char line[LINE_LIM];

    do {
        if (sigint_received) {
            printf("\nEXITING.");
            print_report();
            exit(0);
        }

        printf("ourshell> ");

        // Read user input
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\nEOF.\n");
            print_report();
            exit(0);
        }

        // Remove empty spaces
        line[strcspn(line, "\n")] = '\0';
        char *input = trim(line);

        if (strlen(input) == 0) continue;  // Skip empty input

        if (strcmp(input, "exit") == 0) {
            print_report();
            exit(0);
        }

        if (strcmp(input, "history") == 0) {
            time_t start_real = time(NULL);
            struct timespec start_monotonic, end_monotonic;

            if (clock_gettime(CLOCK_MONOTONIC, &start_monotonic) == -1) {
                perror("clock_gettime failed");
                start_monotonic.tv_sec = 0;
                start_monotonic.tv_nsec = 0;
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
                continue;
            }

            if (pid == 0) { 
                print_history();
                exit(0);
            } else {  
                waitpid(pid, NULL, 0);

                if (clock_gettime(CLOCK_MONOTONIC, &end_monotonic) == -1) {
                    perror("clock_gettime failed");
                    end_monotonic.tv_sec = start_monotonic.tv_sec;
                    end_monotonic.tv_nsec = start_monotonic.tv_nsec;
                }

                // Calculate duration
                double duration = (end_monotonic.tv_sec - start_monotonic.tv_sec) +
                                  (end_monotonic.tv_nsec - start_monotonic.tv_nsec) / 1e9;

                // Add history command to history
                add_history(input, pid, start_real, duration);
            }

            continue;
        }

        char line_copy[LINE_LIM];
        strncpy(line_copy, input, LINE_LIM);
        line_copy[LINE_LIM - 1] = '\0';

        char *segments[PIPE_LIM];
        int n = parse_pipeline(line_copy, segments);

        if (n > 0) {
            launch(segments, n, input);
        }

    } while (1);

    return 0;
}
