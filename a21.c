#include "offline_schedulers.h"

int main() {
    // Define example commands for testing
    Process processes[] = {
        {"sleep 1", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 2", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 3", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 4", false, false, 0, 0, 0, 0, 0, false, 0},
        {"echo 'Hello, World!'", false, false, 0, 0, 0, 0, 0, false, 0},
        {"ls -l", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 3 && echo 'Done sleeping 3'", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 4 && echo 'Done sleeping 4'", false, false, 0, 0, 0, 0, 0, false, 0},
        {"(sleep 5 &)", false, false, 0, 0, 0, 0, 0, false, 0},
        {"(sleep 6 &)", false, false, 0, 0, 0, 0, 0, false, 0},
        {"nonexistent_command", false, false, 0, 0, 0, 0, 0, false, 0},
        {"sleep 1 && non_existent_command", false, false, 0, 0, 0, 0, 0, false, 0}
    };

    int n = sizeof(processes) / sizeof(processes[0]);

    // FCFS Scheduling
    printf("Running FCFS Scheduling...\n");
    FCFS(processes, n);

    // Reset process states
    for (int i = 0; i < n; i++) {
        processes[i].finished = false;
        processes[i].error = false;
        processes[i].start_time = 0;
        processes[i].completion_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].waiting_time = 0;
        processes[i].response_time = 0;
        processes[i].started = false;
    }

    // Round Robin Scheduling with a quantum of 1000 ms
    printf("Running Round Robin Scheduling...\n");
    RoundRobin(processes, n, 1000);

    // Reset process states
    for (int i = 0; i < n; i++) {
        processes[i].finished = false;
        processes[i].error = false;
        processes[i].start_time = 0;
        processes[i].completion_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].waiting_time = 0;
        processes[i].response_time = 0;
        processes[i].started = false;
    }

    // Multi-Level Feedback Queue Scheduling with different quantums and boost time
    printf("Running Multi-Level Feedback Queue Scheduling...\n");
    MultiLevelFeedbackQueue(processes, n, 500, 1000, 1500, 5000);

    return 0;
}
