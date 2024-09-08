// Include necessary headers and defines
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
#include <errno.h>  

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

typedef struct QueueNode {
    int process_index;
    struct QueueNode *next;
} QueueNode;

typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
} Queue;

Queue* create_queue() {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->front = NULL;
    q->rear = NULL;
    return q;
}

void enqueue(Queue *q, int process_index) {
    QueueNode *new_node = (QueueNode *)malloc(sizeof(QueueNode));
    new_node->process_index = process_index;
    new_node->next = NULL;
    if (q->rear == NULL) {
        q->front = new_node;
        q->rear = new_node;
    } else {
        q->rear->next = new_node;
        q->rear = new_node;
    }
}

int dequeue(Queue *q) {
    if (q->front == NULL) {
        return -1;
    }
    QueueNode *temp = q->front;
    int process_index = temp->process_index;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    return process_index;
}

void free_queue(Queue *q) {
    QueueNode *current = q->front;
    QueueNode *next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    free(q);
}

ProcessList process_list = {0};
HistoricalDataList historical_data = {0};
uint64_t scheduler_start_time;

uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000) - scheduler_start_time;
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
    return 1000; // Default 1000 if no historical data
}

Process* add_process(ProcessList *list, const char *command, HistoricalDataList *historical_data, uint64_t arrival_time) {
    if (list->count >= MAX_PROCESSES) {
        printf("Maximum number of processes reached.\n");
        return NULL;
    }

    Process *p = &list->processes[list->count];
    strncpy(p->command, command, MAX_COMMAND_LENGTH - 1);
    p->command[MAX_COMMAND_LENGTH - 1] = '\0';
    p->finished = false;
    p->error = false;
    p->arrival_time = arrival_time;
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
    return p;
}

void update_process_times(Process *p, uint64_t current_time) {
    if (!p->started) {
        p->start_time = current_time;
        // p->response_time = p->start_time - p->arrival_time;
        p->started = true;
    }
    // p->completion_time = current_time;
    // p->turnaround_time = p->completion_time - p->arrival_time;
    p->waiting_time = p->response_time;
}

void execute_process(Process *p, uint64_t quantum) {
    pid_t pid;

    if (p->process_id == -1) {
        // If it's a new process, fork it
        pid = fork();
        if (pid == 0) {
            // Child process
            char *args[] = {"/bin/sh", "-c", p->command, NULL};
            execvp(args[0], args);
            fprintf(stderr, "Error executing command: %s\n", p->command);
            exit(1);  // Exit if execvp fails
        } else if (pid > 0) {
            p->process_id = pid;
        } else {
            perror("fork failed");
            p->finished = true;
            p->error = true;
            return;
        }
    } else {
        // If process was previously stopped, resume it
        if (kill(p->process_id, SIGCONT) < 0) {
            if (errno == ESRCH) {
                // Process doesn't exist anymore
                p->finished = true;
                p->error = true;
                return;
            }
        }
    }

    uint64_t start_time = get_current_time_ms();
    int status;
    uint64_t elapsed_time = 0;

    while (elapsed_time < quantum) {
        pid_t result = waitpid(p->process_id, &status, WNOHANG);
        if (result > 0) {
            // Process finished
            if (WIFEXITED(status)) {
                p->finished = true;
                p->error = (WEXITSTATUS(status) != 0);
            } else if (WIFSIGNALED(status)) {
                p->finished = true;
                p->error = true;
            }
            break;
        } else if (result < 0) {
            if (errno == ECHILD) {
                // No child process
                p->finished = true;
                p->error = true;
            } else {
                perror("waitpid");
            }
            break;
        }
        elapsed_time = get_current_time_ms() - start_time;
    }

    // If the process is still running after the quantum, stop it
    if (!p->finished && elapsed_time >= quantum) {
        kill(p->process_id, SIGSTOP);
    }

    // Update process times after execution
    p->burst_time += elapsed_time;
    p->remaining_time -= elapsed_time;

    uint64_t end_time = get_current_time_ms();
    p->turnaround_time= p->response_time+p->burst_time;
    // Print context switch only once
    print_context_switch(p, start_time, end_time);
}


void check_for_new_input_nonblocking(ProcessList *list, HistoricalDataList *historical_data, uint64_t arrival_time) {
    char new_command[MAX_COMMAND_LENGTH];
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);  // Get the current flags
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);  // Set stdin to non-blocking mode
    while (fgets(new_command, MAX_COMMAND_LENGTH, stdin)) {
        new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
        if (strlen(new_command) > 0) {
            add_process(list, new_command, historical_data, arrival_time);
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);  // Restore original stdin flags
}

void ShortestJobFirst() {
    scheduler_start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    FILE *csv_file = fopen("result_online_SJF.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    while (1) {
        // uint64_t arrival_time = get_current_time_ms();
        check_for_new_input_nonblocking(&process_list, &historical_data,current_time);
        int shortest_job = -1;
        uint64_t min_burst_time = UINT64_MAX;

        for (int i = 0; i < process_list.count; i++) {
            if (!process_list.processes[i].finished) {
                uint64_t estimated_burst_time = get_historical_burst_time(&historical_data, process_list.processes[i].command);
                if (estimated_burst_time < min_burst_time) {
                    shortest_job = i;
                    min_burst_time = estimated_burst_time;
                }
            }
        }

        if (shortest_job != -1) {
            Process *p = &process_list.processes[shortest_job];
            p->response_time = current_time-(p->arrival_time);
            execute_process(p, UINT64_MAX);
            // current_time = get_current_time_ms();
            update_process_times(p, current_time);

            if (p->finished || p->error) {
                completed++;
                if (!p->error) {
                    update_historical_data(&historical_data, p->command, p->burst_time);
                }
                fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
                        p->command,
                        p->finished && !p->error ? "Yes" : "No",
                        p->error ? "Yes" : "No",
                        p->burst_time,
                        p->turnaround_time,
                        p->waiting_time,
                        p->response_time);
                fflush(csv_file);
            }
            current_time+=p->burst_time;
        }

        if (completed == process_list.count && feof(stdin)) {
            break;
        }
    }

    fclose(csv_file);
}

bool is_new_command(HistoricalDataList *list, const char *command) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->data[i].command, command) == 0) {
            return false;  // Command found in historical data
        }
    }
    return true;  // Command not found, so it's new
}

bool check_and_enqueue_new_processes(ProcessList *list, HistoricalDataList *historical_data, Queue *queues[], int quantum0, int quantum1) {
    char new_command[MAX_COMMAND_LENGTH];
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);  // Get the current flags
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);  // Set stdin to non-blocking mode

    bool new_process_added = false;
    uint16_t arrival_time = get_current_time_ms();
    while (fgets(new_command, MAX_COMMAND_LENGTH, stdin)) {
        new_command[strcspn(new_command, "\n")] = 0;  // Remove newline
        if (strlen(new_command) > 0) {
            Process *new_p = add_process(list, new_command,historical_data,arrival_time);
            if (new_p != NULL) {
                uint64_t avg_burst_time = get_historical_burst_time(historical_data, new_command);
                int priority;
                bool check_new = is_new_command(historical_data, new_command);
                if (check_new) {  // No historical data
                    priority = 1;  // Medium priority
                } else {
                    // Assign priority based on average burst time
                    if (avg_burst_time <= quantum0) priority = 0;
                    else if (avg_burst_time <= quantum1) priority = 1;
                    else priority = 2;
                }

                new_p->priority = priority;
                enqueue(queues[priority], list->count - 1);  // Enqueue the index of the new process
                new_process_added = true;
            }
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);  // Restore original stdin flags
    return new_process_added;
}


void handle_finished_process(Process *p, FILE *csv_file, int *completed, HistoricalDataList *historical_data) {
    (*completed)++;
    p->completion_time = get_current_time_ms() - scheduler_start_time;
    p->turnaround_time = p->completion_time - (p->arrival_time - scheduler_start_time);
    p->waiting_time = p->turnaround_time - p->burst_time;

    fprintf(csv_file, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
            p->command,
            p->finished && !p->error ? "Yes" : "No",
            p->error ? "Yes" : "No",
            p->burst_time,
            p->turnaround_time,
            p->waiting_time,
            p->response_time);
    fflush(csv_file);

    if (!p->error) {
        update_historical_data(historical_data, p->command, p->burst_time);
    }
}

void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime) {
    scheduler_start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    uint64_t last_boost_time = 0;
    FILE *csv_file = fopen("result_online_MLFQ.csv", "w");
    if (csv_file == NULL) {
        perror("Error opening CSV file");
        return;
    }
    fprintf(csv_file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    Queue *queues[3];
    for (int i = 0; i < 3; i++) {
        queues[i] = create_queue();
    }

    while (1) {
        current_time = get_current_time_ms() - scheduler_start_time;
        
        bool new_process_added = check_and_enqueue_new_processes(&process_list, &historical_data, queues, quantum0, quantum1);

        if (current_time - last_boost_time >= boostTime) {
            for (int i = 0; i < process_list.count; i++) {
                if (!process_list.processes[i].finished) {
                    process_list.processes[i].priority = 1;
                    enqueue(queues[1], i);
                }
            }
            last_boost_time = current_time;
        }

        for (int priority = 0; priority < 3; priority++) {
            int quantum = (priority == 0) ? quantum0 : (priority == 1) ? quantum1 : quantum2;
            
            if (queues[priority]->front != NULL) {
                int i = dequeue(queues[priority]);
                Process *p = &process_list.processes[i];

                if (p->finished) continue;

                if (!p->started) {
                    p->start_time = current_time;
                    p->response_time = p->start_time - (p->arrival_time - scheduler_start_time);
                    p->started = true;
                }

                uint64_t process_start_time = get_current_time_ms() - scheduler_start_time;
                execute_process(p, quantum);
                uint64_t process_end_time = get_current_time_ms() - scheduler_start_time;

                uint64_t time_spent = process_end_time - process_start_time;
                p->burst_time += time_spent;
                current_time = process_end_time;

                if (p->finished || p->error) {
                    handle_finished_process(p, csv_file, &completed, &historical_data);
                } else {
                    int next_priority = (priority < 2) ? priority + 1 : 2;
                    p->priority = next_priority;
                    enqueue(queues[next_priority], i);
                }

                new_process_added = check_and_enqueue_new_processes(&process_list, &historical_data, queues, quantum0, quantum1);

                current_time = get_current_time_ms() - scheduler_start_time;
                if (current_time - last_boost_time >= boostTime) {
                    for (int i = 0; i < process_list.count; i++) {
                        if (!process_list.processes[i].finished) {
                            process_list.processes[i].priority = 1;
                            enqueue(queues[1], i);
                        }
                    }
                    last_boost_time = current_time;
                }

                if (new_process_added) {
                    break;
                }
            }
        }

        if (completed == process_list.count && feof(stdin)) {
            break;
        }
    }

    for (int i = 0; i < 3; i++) {
        free_queue(queues[i]);
    }
    fclose(csv_file);
}