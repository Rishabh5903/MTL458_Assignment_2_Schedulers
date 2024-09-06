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
    uint64_t arrival_time;
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    uint64_t burst_time;
    bool started;
    int process_id;
    int priority;
    uint64_t remaining_time;
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

uint64_t scheduler_start_time;

uint64_t global_time = 0;

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
    p->arrival_time = get_current_time_ms();
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
        p->response_time = p->start_time - p->arrival_time;
        p->started = true;
    }
    p->completion_time = current_time;
    p->turnaround_time = p->completion_time - p->arrival_time;
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

void check_for_new_input_nonblocking(ProcessList *list, HistoricalDataList *historical_data) {
    char new_command[MAX_COMMAND_LENGTH];
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);  // Get the current flags
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);  // Set stdin to non-blocking mode

    while (fgets(new_command, MAX_COMMAND_LENGTH, stdin)) {
        new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
        if (strlen(new_command) > 0) {
            add_process(list, new_command, historical_data);
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);  // Restore original stdin flags
}

void ShortestJobFirst(ProcessList *list) {
    scheduler_start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    HistoricalDataList historical_data = {0};  // Store historical burst times
    FILE *csv_file = fopen("result_online_SJF.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time,Arrival Time\n");

    while (1) {
        check_for_new_input_nonblocking(list, &historical_data);  // Collect new inputs during each iteration

        int shortest_job = -1;
        uint64_t min_burst_time = UINT64_MAX;

        // Loop to find the shortest job based on historical burst time
        for (int i = 0; i < list->count; i++) {
            if (!list->processes[i].finished) {
                uint64_t estimated_burst_time = get_historical_burst_time(&historical_data, list->processes[i].command);
                if (estimated_burst_time < min_burst_time) {
                    shortest_job = i;
                    min_burst_time = estimated_burst_time;
                }
            }
        }

        if (shortest_job != -1) {
            Process *p = &list->processes[shortest_job];
            execute_process(p, UINT64_MAX);  // Run until completion
            current_time = get_current_time_ms();
            update_process_times(p, current_time);

            if (p->finished) {
                completed++;
                if (!p->error) {
                    // Update historical burst time only if no error occurred
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                // Log process data to CSV
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time,
                        p->arrival_time);
                fflush(csv_file);
            }
        }

        // Break when all processes are completed
        if (completed == list->count && feof(stdin)) {
            break;
        }
    }

    fclose(csv_file);
}

void MultiLevelFeedbackQueue(ProcessList *list, int quantum0, int quantum1, int quantum2, int boostTime) {
    scheduler_start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    uint64_t last_boost_time = current_time;
    HistoricalDataList historical_data = {0};
    FILE *csv_file = fopen("result_online_MLFQ.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time,Arrival Time\n");

    while (1) {
        check_for_new_input_nonblocking(list, &historical_data);

        current_time = get_current_time_ms();
        if (current_time - last_boost_time >= boostTime) {
            for (int i = 0; i < list->count; i++) {
                if (!list->processes[i].finished) {
                    list->processes[i].priority = 1;  // Reset to medium priority
                }
            }
            last_boost_time = current_time;
        }

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
            current_time = get_current_time_ms();
            update_process_times(p, current_time);

            if (p->finished) {
                completed++;
                if (!p->error) {
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time,
                        p->arrival_time);
                fflush(csv_file);
            } else {
                p->priority = (p->priority < 2) ? p->priority + 1 : 2;
            }
        } else if (completed == list->count && feof(stdin)) {
            break;
        }

        for (int i = 0; i < list->count; i++) {
            Process *p = &list->processes[i];
            if (!p->finished && !p->started) {
                uint64_t avg_burst_time = get_historical_burst_time(&historical_data, p->command);
                if (avg_burst_time < 1000) {
                    p->priority = 0;
                } else if (avg_burst_time > 2000) {
                    p->priority = 2;
                }
            }
        }
    }

    fclose(csv_file);
}