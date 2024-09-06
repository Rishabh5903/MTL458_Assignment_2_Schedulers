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
#include <time.h>
#include <stdint.h>

#define MAX_PROCESSES 100
#define MAX_COMMAND_LENGTH 256

typedef struct {
    char command[MAX_COMMAND_LENGTH];
    bool finished;
    bool error;
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    uint64_t burst_time;
    bool started;
    int process_id;
    int priority;  // For MLFQ
    uint64_t remaining_time;  // For SRTF
} Process;

typedef struct {
    Process processes[MAX_PROCESSES];
    int count;
} ProcessList;

typedef struct {
    char command[MAX_COMMAND_LENGTH];
    uint64_t avg_burst_time;
    int count;
} HistoricalData;

typedef struct {
    HistoricalData data[MAX_PROCESSES];
    int count;
} HistoricalDataList;

uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void print_context_switch(Process *p, uint64_t start_time, uint64_t end_time) {
    printf("%s|%lu|%lu\n", p->command, start_time, end_time);
}

void update_historical_data(HistoricalDataList *list, const char *command, uint64_t burst_time) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->data[i].command, command) == 0) {
            list->data[i].avg_burst_time = (list->data[i].avg_burst_time * list->data[i].count + burst_time) / (list->data[i].count + 1);
            list->data[i].count++;
            return;
        }
    }
    
    if (list->count < MAX_PROCESSES) {
        strcpy(list->data[list->count].command, command);
        list->data[list->count].avg_burst_time = burst_time;
        list->data[list->count].count = 1;
        list->count++;
    }
}

uint64_t get_historical_burst_time(HistoricalDataList *list, const char *command) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->data[i].command, command) == 0) {
            return list->data[i].avg_burst_time;
        }
    }
    return 1000; // Default 1 second if no historical data
}

void add_process(ProcessList *list, const char *command, HistoricalDataList *historical_data) {
    if (list->count >= MAX_PROCESSES) {
        printf("Maximum number of processes reached.\n");
        return;
    }

    Process *p = &list->processes[list->count];
    strncpy(p->command, command, MAX_COMMAND_LENGTH - 1);
    p->command[MAX_COMMAND_LENGTH - 1] = '\0';
    p->finished = false;
    p->error = false;
    p->start_time = 0;
    p->completion_time = 0;
    p->turnaround_time = 0;
    p->waiting_time = 0;
    p->response_time = 0;
    p->burst_time = 0;
    p->started = false;
    p->process_id = -1;
    p->priority = 1;  // Medium priority for MLFQ
    p->remaining_time = get_historical_burst_time(historical_data, command);

    list->count++;
}

void update_process_times(Process *p, uint64_t current_time) {
    if (!p->started) {
        p->start_time = current_time;
        p->response_time = current_time - p->start_time;
        p->started = true;
    }
    p->completion_time = current_time;
    p->turnaround_time = p->completion_time - p->start_time;
    p->waiting_time = p->turnaround_time - p->burst_time;
}

void execute_process(Process *p, uint64_t quantum) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char *args[] = {"/bin/sh", "-c", p->command, NULL};
        execvp(args[0], args);
        exit(1);  // Exit if execvp fails
    } else if (pid > 0) {
        // Parent process
        p->process_id = pid;
        uint64_t start_time = get_current_time_ms();
        int status;
        uint64_t elapsed_time = 0;

        while (elapsed_time < quantum) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result > 0) {
                // Process finished
                p->finished = true;
                p->error = WIFEXITED(status) ? (WEXITSTATUS(status) != 0) : true;
                break;
            } else if (result < 0) {
                // Error occurred
                perror("waitpid");
                p->finished = true;
                p->error = true;
                break;
            }
            usleep(1000);  // Sleep for 1ms
            elapsed_time = get_current_time_ms() - start_time;
        }

        if (!p->finished) {
            kill(pid, SIGSTOP);
        }

        p->burst_time += elapsed_time;
        p->remaining_time -= elapsed_time;
        
        uint64_t end_time = get_current_time_ms();
        print_context_switch(p, start_time, end_time);
    } else {
        // Fork failed
        perror("fork failed");
        p->finished = true;
        p->error = true;
    }
}





void ShortestJobFirst(ProcessList *list) {
    uint64_t current_time = 0;
    int completed = 0;
    HistoricalDataList historical_data = {0};
    FILE *csv_file = fopen("result_online_SJF.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    while (1) {
        // Check for new processes
        char new_command[MAX_COMMAND_LENGTH];
        if (fgets(new_command, MAX_COMMAND_LENGTH, stdin) != NULL) {
            new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
            if (strlen(new_command) > 0) {
                add_process(list, new_command, &historical_data);
            }
        }

        // Find the process with the shortest estimated burst time
        int shortest_job = -1;
        uint64_t min_burst_time = UINT64_MAX;

        for (int i = 0; i < list->count; i++) {
            if (!list->processes[i].finished && list->processes[i].remaining_time < min_burst_time) {
                shortest_job = i;
                min_burst_time = list->processes[i].remaining_time;
            }
        }

        if (shortest_job != -1) {
            Process *p = &list->processes[shortest_job];
            execute_process(p, p->remaining_time);
            current_time += p->burst_time;
            update_process_times(p, current_time);

            if (p->finished) {
                completed++;
                if (!p->error) {
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                // Write result to CSV immediately
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time);
                fflush(csv_file);  // Ensure data is written immediately
            }
        } else {
            // No unfinished processes and no new input, we can exit
            if (completed == list->count && feof(stdin)) {
                break;
            }
        }
    }

    fclose(csv_file);
}





void ShortestRemainingTimeFirst(ProcessList *list) {
    uint64_t current_time = 0;
    int completed = 0;
    HistoricalDataList historical_data = {0};
    FILE *csv_file = fopen("result_online_SRTF.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    while (1) {
        // Check for new processes
        char new_command[MAX_COMMAND_LENGTH];
        if (fgets(new_command, MAX_COMMAND_LENGTH, stdin) != NULL) {
            new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
            if (strlen(new_command) > 0) {
                add_process(list, new_command, &historical_data);
            }
        }

        // Find the process with the shortest remaining time
        int shortest_remaining = -1;
        uint64_t min_remaining_time = UINT64_MAX;

        for (int i = 0; i < list->count; i++) {
            if (!list->processes[i].finished && list->processes[i].remaining_time < min_remaining_time) {
                shortest_remaining = i;
                min_remaining_time = list->processes[i].remaining_time;
            }
        }

        if (shortest_remaining != -1) {
            Process *p = &list->processes[shortest_remaining];
            execute_process(p, 100);  // Execute for 100ms time slice
            current_time += 100;
            update_process_times(p, current_time);

            if (p->finished) {
                completed++;
                if (!p->error) {
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                // Write result to CSV immediately
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time);
                fflush(csv_file);  // Ensure data is written immediately
            }
        } else {
            // No unfinished processes and no new input, we can exit
            if (completed == list->count && feof(stdin)) {
                break;
            }
        }
    }

    fclose(csv_file);
}





void MultiLevelFeedbackQueue(ProcessList *list, int quantum0, int quantum1, int quantum2, int boostTime) {
    uint64_t current_time = 0;
    int completed = 0;
    uint64_t last_boost_time = 0;
    HistoricalDataList historical_data = {0};
    FILE *csv_file = fopen("result_online_MLFQ.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    while (1) {
        // Check for new processes
        char new_command[MAX_COMMAND_LENGTH];
        if (fgets(new_command, MAX_COMMAND_LENGTH, stdin) != NULL) {
            new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
            if (strlen(new_command) > 0) {
                add_process(list, new_command, &historical_data);
            }
        }

        // Priority boost
        if (current_time - last_boost_time >= boostTime) {
            for (int i = 0; i < list->count; i++) {
                if (!list->processes[i].finished) {
                    list->processes[i].priority = 1;  // Reset to medium priority
                }
            }
            last_boost_time = current_time;
        }

        // Find the highest priority process
        int highest_priority = -1;
        int min_priority = 3;

        for (int i = 0; i < list->count; i++) {
            if (!list->processes[i].finished && list->processes[i].priority < min_priority) {
                highest_priority = i;
                min_priority = list->processes[i].priority;
            }
        }

        if (highest_priority != -1) {
            Process *p = &list->processes[highest_priority];
            int quantum = (p->priority == 0) ? quantum0 : (p->priority == 1) ? quantum1 : quantum2;

            execute_process(p, quantum);
            current_time += (p->burst_time > quantum) ? quantum : p->burst_time;
            update_process_times(p, current_time);

            if (p->finished) {
                completed++;
                if (!p->error) {
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                // Write result to CSV immediately
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time);
                fflush(csv_file);  // Ensure data is written immediately
            } else {
                // Adjust priority
                p->priority = (p->priority < 2) ? p->priority + 1 : 2;
            }
        } else {
            // No unfinished processes and no new input, we can exit
            if (completed == list->count && feof(stdin)) {
                break;
            }
        }

        // Update priority based on average burst time for new processes
        for (int i = 0; i < list->count; i++) {
            Process *p = &list->processes[i];
            if (!p->finished && p->priority == 1) { // Only for new processes in mid-priority
                uint64_t avg_burst_time = get_historical_burst_time(&historical_data, p->command);
                if (avg_burst_time < 1000) {  // If average burst time is less than 1 second
                    p->priority = 0;  // Promote to highest priority
                } else if (avg_burst_time > 2000) {  // If greater than 2 seconds
                    p->priority = 2;  // Demote to lowest priority
                }
            }
        }
    }

    fclose(csv_file);
}
