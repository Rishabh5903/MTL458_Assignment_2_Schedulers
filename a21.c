// test_offline_schedulers.c

#include <stdio.h>
#include <stdlib.h>
#include "offline_schedulers.h"  // Assuming the main scheduler code is in this header

int main() {
    Process processes[] = {
        {"Process1", false, false, 0, 0, 0, 0, 0, false, 0, 1, 1, 0},
        {"Process2", false, false, 0, 0, 0, 0, 0, false, 0, 2, 2, 0},
        {"Process3", false, false, 0, 0, 0, 0, 0, false, 0, 1, 1, 0}
    };
    int n = sizeof(processes) / sizeof(Process);

    // Test FCFS
    printf("Testing FCFS Scheduler...\n");
    FCFS(processes, n);

    // Reset the state of processes for next test
    for (int i = 0; i < n; i++) {
        processes[i].finished = false;
        processes[i].started = false;
        processes[i].remaining_time = processes[i].burst_time;
    }

    // Test Round Robin with a time quantum of 1
    printf("Testing Round Robin Scheduler...\n");
    RoundRobin(processes, n, 1);

    // Reset the state of processes for next test
    for (int i = 0; i < n; i++) {
        processes[i].finished = false;
        processes[i].started = false;
        processes[i].remaining_time = processes[i].burst_time;
    }

    // Test MLFQ with quantums 1, 2, and 4 for three different levels
    printf("Testing MLFQ Scheduler...\n");
    MultiLevelFeedbackQueue(processes, n, 1, 2, 4);

    return 0;
}
