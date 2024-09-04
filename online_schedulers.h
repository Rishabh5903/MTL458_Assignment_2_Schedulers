#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

#define MAX_PROCESSES 100
#define MAX_COMMAND_LENGTH 100

typedef struct {
    char *command;
    bool finished;
    bool error;    
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    bool started; 
    int process_id;
    int burst_time;
    int remaining_time;
    int priority;
} Process;

// Helper functions
uint64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

void execute_process(Process *p) {
    p->start_time = get_current_time_ms();
    p->started = true;
    
    // Simulate process execution
    sleep(1);  // Simulate 1 second of execution
    
    p->completion_time = get_current_time_ms();
    p->finished = true;
    p->turnaround_time = p->completion_time - p->start_time;
    p->waiting_time = p->turnaround_time - 1000;  // Assuming 1 second burst time
    p->response_time = p->start_time - p->start_time;  // 0 for FCFS
}

void write_to_csv(const char *filename, Process *processes[], int n) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }
    
    fprintf(fp, "Command,Finished,Error,Completion Time,Turnaround Time,Waiting Time,Response Time\n");
    
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                processes[i]->command,
                processes[i]->finished ? "Yes" : "No",
                processes[i]->error ? "Yes" : "No",
                processes[i]->completion_time,
                processes[i]->turnaround_time,
                processes[i]->waiting_time,
                processes[i]->response_time);
    }
    
    fclose(fp);
}

Process* read_process_from_input() {
    char command[MAX_COMMAND_LENGTH];
    if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
        return NULL;
    }
    command[strcspn(command, "\n")] = 0;  // Remove newline
    
    Process *p = malloc(sizeof(Process));
    p->command = strdup(command);
    p->finished = false;
    p->error = false;
    p->started = false;
    p->burst_time = 1;  // Default burst time
    p->remaining_time = 1;
    p->priority = 1;  // Medium priority for MLFQ
    
    return p;
}

void update_burst_time(Process *p) {
    // This is a simplified version. In a real scenario, you'd use more sophisticated estimation techniques.
    if (p->finished && !p->error) {
        p->burst_time = (p->burst_time + (p->completion_time - p->start_time) / 1000) / 2;
    }
}

// Scheduling algorithms
void ShortestJobFirst() {
    Process *processes[MAX_PROCESSES];
    int n = 0;
    
    while (1) {
        Process *new_process = read_process_from_input();
        if (new_process == NULL) break;
        
        processes[n++] = new_process;
        
        // Sort processes based on burst time
        for (int i = n - 1; i > 0 && processes[i]->burst_time < processes[i-1]->burst_time; i--) {
            Process *temp = processes[i];
            processes[i] = processes[i-1];
            processes[i-1] = temp;
        }
        
        // Execute the shortest job
        execute_process(processes[0]);
        printf("%s|%lu|%lu\n", processes[0]->command, processes[0]->start_time, processes[0]->completion_time);
        
        update_burst_time(processes[0]);
        
        // Remove the executed process
        free(processes[0]->command);
        free(processes[0]);
        for (int i = 0; i < n - 1; i++) {
            processes[i] = processes[i+1];
        }
        n--;
    }
    
    write_to_csv("result_online_SJF.csv", processes, n);
}

void ShortestRemainingTimeFirst() {
    Process *processes[MAX_PROCESSES];
    int n = 0;
    int current_time = 0;
    
    while (1) {
        Process *new_process = read_process_from_input();
        if (new_process == NULL && n == 0) break;
        
        if (new_process != NULL) {
            processes[n++] = new_process;
        }
        
        // Find the process with the shortest remaining time
        int shortest = 0;
        for (int i = 1; i < n; i++) {
            if (processes[i]->remaining_time < processes[shortest]->remaining_time) {
                shortest = i;
            }
        }
        
        // Execute the process for 1 time unit
        if (!processes[shortest]->started) {
            processes[shortest]->start_time = get_current_time_ms();
            processes[shortest]->started = true;
            processes[shortest]->response_time = processes[shortest]->start_time - processes[shortest]->start_time;
        }
        
        sleep(1);
        current_time++;
        processes[shortest]->remaining_time--;
        
        printf("%s|%d|%d\n", processes[shortest]->command, current_time - 1, current_time);
        
        if (processes[shortest]->remaining_time == 0) {
            processes[shortest]->finished = true;
            processes[shortest]->completion_time = get_current_time_ms();
            processes[shortest]->turnaround_time = processes[shortest]->completion_time - processes[shortest]->start_time;
            processes[shortest]->waiting_time = processes[shortest]->turnaround_time - processes[shortest]->burst_time;
            
            update_burst_time(processes[shortest]);
            
            // Remove the finished process
            free(processes[shortest]->command);
            free(processes[shortest]);
            for (int i = shortest; i < n - 1; i++) {
                processes[i] = processes[i+1];
            }
            n--;
        }
    }
    
    write_to_csv("result_online_SRTF.csv", processes, n);
}

void MultiLevelFeedbackQueueOnline(int quantum0, int quantum1, int quantum2, int boostTime) {
    Process *processes[MAX_PROCESSES];
    int n = 0;
    int current_time = 0;
    int time_since_last_boost = 0;

    while (1) {
        Process *new_process = read_process_from_input();
        if (new_process == NULL && n == 0) break;

        if (new_process != NULL) {
            processes[n++] = new_process;
        }

// Execute processes based on priority and quantum
        for (int priority = 0; priority < 3; priority++) {
            int quantum = (priority == 0) ? quantum0 : (priority == 1) ? quantum1 : quantum2;

            for (int i = 0; i < n; i++) {
                if (processes[i]->priority == priority) {
                    if (!processes[i]->started) {
                        processes[i]->start_time = get_current_time_ms();
                        processes[i]->started = true;
                        processes[i]->response_time = processes[i]->start_time - processes[i]->start_time;
                    }

                    int time_to_run = quantum < processes[i]->remaining_time ? quantum : processes[i]->remaining_time;
                    sleep(time_to_run);

                    processes[i]->remaining_time -= time_to_run;
                    current_time += time_to_run;
                    time_since_last_boost += time_to_run;

                    printf("%s|%d|%d\n", processes[i]->command, current_time - time_to_run, current_time);

                    if (processes[i]->remaining_time == 0) {
                        processes[i]->finished = true;
                        processes[i]->completion_time = get_current_time_ms();
                        processes[i]->turnaround_time = processes[i]->completion_time - processes[i]->start_time;
                        processes[i]->waiting_time = processes[i]->turnaround_time - processes[i]->burst_time;

                        update_burst_time(processes[i]);

                        // Remove the finished process
                        free(processes[i]->command);
                        free(processes[i]);
                        for (int j = i; j < n - 1; j++) {
                            processes[j] = processes[j + 1];
                        }
                        n--;
                        i--;  // Adjust index since we removed an element
                    } else {
                        // Move to lower priority queue if not finished
                        processes[i]->priority = (processes[i]->priority < 2) ? processes[i]->priority + 1 : 2;
                    }
                }
            }
        }

        // Priority boost
        if (time_since_last_boost >= boostTime) {
            for (int i = 0; i < n; i++) {
                processes[i]->priority = 0;
            }
            time_since_last_boost = 0;
        }
    }

    write_to_csv("result_online_MLFQ.csv", processes, n);
}