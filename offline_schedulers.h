#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

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

void write_to_csv(const char *filename, Process p[], int n) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }
    
    fprintf(fp, "Command,Finished,Error,Completion Time,Turnaround Time,Waiting Time,Response Time\n");
    
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                p[i].command,
                p[i].finished ? "Yes" : "No",
                p[i].error ? "Yes" : "No",
                p[i].completion_time,
                p[i].turnaround_time,
                p[i].waiting_time,
                p[i].response_time);
    }
    
    fclose(fp);
}

// Scheduling algorithms
void FCFS(Process p[], int n) {
    for (int i = 0; i < n; i++) {
        execute_process(&p[i]);
        printf("%s|%lu|%lu\n", p[i].command, p[i].start_time, p[i].completion_time);
    }
    write_to_csv("result_offline_FCFS.csv", p, n);
}

void RoundRobin(Process p[], int n, int quantum) {
    int completed = 0;
    int current_time = 0;
    
    while (completed < n) {
        for (int i = 0; i < n; i++) {
            if (!p[i].finished) {
                if (!p[i].started) {
                    p[i].start_time = get_current_time_ms();
                    p[i].started = true;
                    p[i].response_time = p[i].start_time - p[i].start_time;  // 0 for first execution
                }
                
                int execution_time = (p[i].remaining_time < quantum) ? p[i].remaining_time : quantum;
                sleep(execution_time);
                current_time += execution_time;
                p[i].remaining_time -= execution_time;
                
                if (p[i].remaining_time == 0) {
                    p[i].finished = true;
                    p[i].completion_time = get_current_time_ms();
                    p[i].turnaround_time = p[i].completion_time - p[i].start_time;
                    p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;
                    completed++;
                }
                
                printf("%s|%d|%d\n", p[i].command, current_time - execution_time, current_time);
            }
        }
    }
    
    write_to_csv("result_offline_RR.csv", p, n);
}

void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime) {
    int completed = 0;
    int current_time = 0;
    int time_since_last_boost = 0;
    
    while (completed < n) {
        for (int priority = 0; priority < 3; priority++) {
            int quantum = (priority == 0) ? quantum0 : (priority == 1) ? quantum1 : quantum2;
            
            for (int i = 0; i < n; i++) {
                if (!p[i].finished && p[i].priority == priority) {
                    if (!p[i].started) {
                        p[i].start_time = get_current_time_ms();
                        p[i].started = true;
                        p[i].response_time = p[i].start_time - p[i].start_time;  // 0 for first execution
                    }
                    
                    int execution_time = (p[i].remaining_time < quantum) ? p[i].remaining_time : quantum;
                    sleep(execution_time);
                    current_time += execution_time;
                    time_since_last_boost += execution_time;
                    p[i].remaining_time -= execution_time;
                    
                    if (p[i].remaining_time == 0) {
                        p[i].finished = true;
                        p[i].completion_time = get_current_time_ms();
                        p[i].turnaround_time = p[i].completion_time - p[i].start_time;
                        p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;
                        completed++;
                    } else {
                        p[i].priority = (p[i].priority < 2) ? p[i].priority + 1 : 2;
                    }
                    
                    printf("%s|%d|%d\n", p[i].command, current_time - execution_time, current_time);
                }
            }
        }
        
        // Priority boost
        if (time_since_last_boost >= boostTime) {
            for (int i = 0; i < n; i++) {
                if (!p[i].finished) {
                    p[i].priority = 0;
                }
            }
            time_since_last_boost = 0;
        }
    }
    
    write_to_csv("result_offline_MLFQ.csv", p, n);
}