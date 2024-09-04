#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "online_schedulers.h"

// Mock function to simulate user input for processes
Process* mock_read_process_from_input(const char* command) {
    if (command == NULL) {
        return NULL;
    }

    Process *p = malloc(sizeof(Process));
    p->command = strdup(command);
    p->finished = false;
    p->error = false;
    p->started = false;
    p->burst_time = 1;  // Default burst time for testing
    p->remaining_time = 1;
    p->priority = 1;  // Medium priority for MLFQ

    return p;
}

int main() {
    // Test SJF
    printf("Testing Shortest Job First Scheduler...\n");
    // Mock input commands
    Process *processes[] = {
        mock_read_process_from_input("Process1"),
        mock_read_process_from_input("Process2"),
        mock_read_process_from_input("Process3")
    };
    int n = sizeof(processes) / sizeof(Process*);
    
    for (int i = 0; i < n; i++) {
        if (processes[i] != NULL) {
            execute_process(processes[i]);
            printf("%s|%lu|%lu\n", processes[i]->command, processes[i]->start_time, processes[i]->completion_time);
            free(processes[i]->command);
            free(processes[i]);
        }
    }
    
    // Test SRTF
    printf("Testing Shortest Remaining Time First Scheduler...\n");
    ShortestRemainingTimeFirst();

    // Test Online MLFQ with quantums 1, 2, and 4 for three different levels, and a boost time of 10
    printf("Testing Online MLFQ Scheduler...\n");
    MultiLevelFeedbackQueueOnline(1, 2, 4, 10);

    return 0;
}