#include "offline_schedulers.h"

int main() {
    // Define example commands for testing
    Process processes[] = {
        {"sleep 2"},  // Simple loop
        {"pwd"},  // Simple loop
        {"sleep 2"},  // Simple loop
        // {"sleep 1"},  // Simple loop
        // {"for i in {1..50000}; do echo 'Looping'; done"},  // Loop with echo
        // {"bdtrhe"},  // List directory contents
        // {"date"},  // Print current date
        // {"echo 'Task completed!'"},  // Simple echo statement
    };

    int n = sizeof(processes) / sizeof(processes[0]);

    // Set a small quantum to force pausing and resuming
    int quantum = 100;  // 100 ms quantum for quick switching

    // FCFS Scheduling (optional)
    printf("Running FCFS Scheduling...\n");
    FCFS(processes, n);


    // Round Robin Scheduling with the small quantum
    printf("Running Round Robin Scheduling...\n");
    RoundRobin(processes, n, quantum);


    // Multi-Level Feedback Queue Scheduling (optional)
    printf("Running Multi-Level Feedback Queue Scheduling...\n");
    MultiLevelFeedbackQueue(processes, n, 100, 200, 300, 1000);

    return 0;
}
