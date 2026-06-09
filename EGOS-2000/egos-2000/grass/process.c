/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"

#define MLFQ_NLEVELS          5
#define MLFQ_RESET_PERIOD     10000000         /* 10 seconds */
#define MLFQ_LEVEL_RUNTIME(x) (x + 1) * 100000 /* e.g., 100ms for level 0 */
extern struct process proc_set[MAX_NPROCESS + 1];

static void proc_set_status(int pid, enum proc_status status) {
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

int proc_alloc() {
    static uint curr_pid = 0;
    for (uint i = 1; i <= MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;

/////////////////////////////////////////////////////////////////////////////////////////////////
            /* Student's code goes here (Preemptive Scheduler | System Call). */

            //Initializing when a new process is allocated
            proc_set[i].creation_time = mtime_get();      //Record when process was created
            proc_set[i].first_run_time = 0;
            proc_set[i].cpu_time = 0;
            proc_set[i].timer_interrupt_count = 0;
            proc_set[i].mlfq_level = 0;             //All processes start at highest priority level 0
            proc_set[i].mlfq_runtime_on_level = 0;

            /* Student's code ends here. */
//////////////////////////////////////////////////////////////////////////////////////////////////
            return curr_pid;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}


void proc_free(int pid) {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /* Student's code goes here (Preemptive Scheduler). */

     /* Print the lifecycle statistics of the terminated process or processes. */
    if (pid != GPID_ALL) {
        // Free a single specific process
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == pid && proc_set[i].pid >= GPID_USER_START) {
                //divide by 1000 for microseconds to ms conversion
                int turnaround = (int)((mtime_get() - proc_set[i].creation_time) / 1000);   //turnaround time: total time from creation to termination
                int response = (int)((proc_set[i].first_run_time - proc_set[i].creation_time) / 1000);   //response time: time from creation to first execution
                int cpu_time = (int)(proc_set[i].cpu_time / 1000); //total execution time on CPU
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                     pid, proc_set[i].timer_interrupt_count, turnaround, response, cpu_time);
                break;
            }
        }
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
    } else {
        /* Free all user processes. */
        for (uint i = 0; i < MAX_NPROCESS; i++)
            if (proc_set[i].pid >= GPID_USER_START &&
                proc_set[i].status != PROC_UNUSED) {
                int turnaround = (int)((mtime_get() - proc_set[i].creation_time) /1000);
                int response = (int)((proc_set[i].first_run_time - proc_set[i].creation_time) /1000);
                int cpu_time = (int)(proc_set[i].cpu_time /1000);
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                     proc_set[i].pid, proc_set[i].timer_interrupt_count, turnaround, response, cpu_time);
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].status = PROC_UNUSED;
            }
    }
     /* Student's code ends here. */
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mlfq_update_level(struct process* p, ulonglong runtime) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* Student's code goes here (Preemptive Scheduler). */

    //mlfq_update_level() called after each timer interrupt to potentially demote a process

    p->mlfq_runtime_on_level += runtime;       //update the process time spent on current level
    if (p->mlfq_level < MLFQ_NLEVELS-1 && p->mlfq_runtime_on_level >= MLFQ_LEVEL_RUNTIME(p->mlfq_level)) {
        //check if process exceeded its time quantum at current level
        p->mlfq_level++;  //demote process to next lower level
        p->mlfq_runtime_on_level = 0;   //reset process runtime for next level
    }

    /* Student's code ends here. */
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mlfq_reset_level() {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* Student's code goes here (Preemptive Scheduler). */

    //mlfq_reset_level() resets all processes and shell(for immediate response to user input) to Level 0 every MLFQ_RESET_PERIOD
    if (!earth->tty_input_empty()) {
        /* Reset the level of GPID_SHELL if there is pending keyboard input */
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == GPID_SHELL) {    //search for shell process
                proc_set[i].mlfq_level = 0;         //reset shell to highest priority level 0 
                proc_set[i].mlfq_runtime_on_level = 0;
                break;
            }
        }
    }

    static ulonglong MLFQ_last_reset_time = 0;
    /* Reset the level of all processes every MLFQ_RESET_PERIOD microseconds. */
    ulonglong current_time = mtime_get();
    if (current_time - MLFQ_last_reset_time >= MLFQ_RESET_PERIOD) {
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].status != PROC_UNUSED) {  //reset process levels only for those in use
                proc_set[i].mlfq_level = 0;
                proc_set[i].mlfq_runtime_on_level = 0;
            }
        }
        MLFQ_last_reset_time = current_time;
    }

    /* Student's code ends here. */
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void proc_sleep(int pid, uint usec) {
    /* Student's code goes here (System Call & Protection). */

    /* Update the sleep-related fields in the struct process for process pid. */

    /* Student's code ends here. */
}

void proc_coresinfo() {
    /* Student's code goes here (Multicore & Locks). */

    /* Print out the pid of the process running on each CPU core. */

    /* Student's code ends here. */
}
