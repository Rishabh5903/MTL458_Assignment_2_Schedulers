#include "offline_schedulers.h"

int main() {
    // Define example commands for testing
    Process processes[] = {
        {"dd if=/dev/zero of=testfile bs=1M count=100", false, false, 0, 0, 0, 0, 0, false, 0},  // Simple loop
        {"for i in {1..50000}; do echo 'Looping'; done", false, false, 0, 0, 0, 0, 0, false, 0},  // Loop with echo
        {"ls -l", false, false, 0, 0, 0, 0, 0, false, 0},  // List directory contents
        {"date", false, false, 0, 0, 0, 0, 0, false, 0},  // Print current date
        {"echo 'Task completed!'", false, false, 0, 0, 0, 0, 0, false, 0},  // Simple echo statement
    };

    int n = sizeof(processes) / sizeof(processes[0]);

    // Set a small quantum to force pausing and resuming
    int quantum = 100;  // 100 ms quantum for quick switching

    // FCFS Scheduling (optional)
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

    // Round Robin Scheduling with the small quantum
    printf("Running Round Robin Scheduling...\n");
    RoundRobin(processes, n, quantum);

    // Reset process states for Multi-Level Feedback Queue (optional)
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

    // Multi-Level Feedback Queue Scheduling (optional)
    printf("Running Multi-Level Feedback Queue Scheduling...\n");
    MultiLevelFeedbackQueue(processes, n, 100, 200, 300, 1000);

    return 0;
}
