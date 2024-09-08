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
#include <time.h>

// Structure to represent a process
typedef struct {
    char *command;
    bool finished;
    bool error;
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    uint64_t last_executed_time;
    uint64_t burst_time;           
    bool started;
    int process_id;
} Process;

// Function to execute a command
void execute_command(const char* command) {
    // Redirect output to /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull == -1) {
        exit(1);
    }
    
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    // Execute the command using /bin/sh
    char *args[] = {"/bin/sh", "-c", (char*)command, NULL};
    execvp(args[0], args);
    exit(1);
}

// Function to get current time in milliseconds
uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// Queue node structure for process queue
typedef struct QueueNode {
    int process_index;
    struct QueueNode *next;
} QueueNode;

// Queue structure
typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
} Queue;

// Function to create a new queue
Queue* create_queue() {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->front = NULL;
    q->rear = NULL;
    return q;
}

// Function to add a process to the queue
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

// Function to remove and return a process from the queue
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

// Function to free the memory used by the queue
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

// Function to write results to a CSV file
void write_results_to_csv(Process p[], int n, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    for (int i = 0; i < n; i++) {
        fprintf(fp, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
                p[i].command,
                p[i].finished && !p[i].error ? "Yes" : "No",
                p[i].error ? "Yes" : "No",
                p[i].burst_time,
                p[i].turnaround_time,
                p[i].waiting_time,
                p[i].response_time);
    }

    fclose(fp);
}

// Function to print context switch information
void print_context_switch(const char* command, uint64_t start_time, uint64_t end_time) {
    printf("%s|%lu|%lu\n", command, start_time, end_time);
}

// First-Come, First-Served (FCFS) scheduling algorithm
void FCFS(Process p[], int n) {
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time = 0;

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

    // Initialize process data
    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].completion_time = 0;
        p[i].turnaround_time = 0;
        p[i].waiting_time = 0;
        p[i].response_time = 0;
        p[i].burst_time = 0;
        process_pids[i] = -1;
    }

    // Execute processes in order
    for (int i = 0; i < n; i++) {
        p[i].start_time = current_time;
        p[i].response_time = p[i].start_time;
        p[i].started = true;

        uint64_t process_start_time = get_current_time_ms() - start_time;

        // Fork and execute the process
        process_pids[i] = fork();
        if (process_pids[i] == 0) {
            execute_command(p[i].command);
        } else if (process_pids[i] < 0) {
            p[i].finished = false;
            p[i].error = true;
            continue;
        }

        int status;
        uint64_t execution_start_time = get_current_time_ms();
        waitpid(process_pids[i], &status, 0);
        uint64_t execution_end_time = get_current_time_ms();

        // Calculate process metrics
        p[i].completion_time = execution_end_time - start_time;
        p[i].turnaround_time = p[i].completion_time;
        p[i].waiting_time = p[i].response_time;
        p[i].burst_time = execution_end_time - execution_start_time;

        // Check process exit status
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                p[i].finished = true;
                p[i].error = false;
            } else {
                p[i].finished = false;
                p[i].error = true;
            }
        } else {
            p[i].finished = false;
            p[i].error = true;
        }

        uint64_t process_end_time = get_current_time_ms() - start_time;
        print_context_switch(p[i].command, process_start_time, process_end_time);

        current_time = p[i].completion_time;
    }

    free(process_pids);
    write_results_to_csv(p, n, "result_offline_FCFS.csv");
}

// Round Robin (RR) scheduling algorithm
void RoundRobin(Process p[], int n, int quantum) {
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    Queue *ready_queue = create_queue();

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

    // Initialize process data and queue
    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].last_executed_time = 0;
        p[i].burst_time = 0;
        p[i].waiting_time = 0;
        process_pids[i] = -1;
        enqueue(ready_queue, i);
    }

    // Execute processes in round-robin fashion
    while (completed < n) {
        int i = dequeue(ready_queue);
        if (i == -1) break;

        if (p[i].finished) {
            continue;
        }

        if (!p[i].started) {
            p[i].start_time = current_time;
            p[i].response_time = current_time;
            p[i].started = true;
            p[i].last_executed_time = current_time;
        }

        uint64_t process_start_time = get_current_time_ms() - start_time;

        // Fork or continue the process
        if (process_pids[i] == -1) {
            process_pids[i] = fork();
            if (process_pids[i] == 0) {
                execute_command(p[i].command);
            } else if (process_pids[i] < 0) {
                p[i].finished = true;
                p[i].error = true;
                completed++;
                continue;
            }
        } else {
            kill(process_pids[i], SIGCONT);
        }

        int status;
        uint64_t start_execution = get_current_time_ms();
        uint64_t elapsed_time = 0;
        bool process_finished = false;

        // Execute the process for the quantum or until it finishes
        while (elapsed_time < quantum) {
            pid_t result = waitpid(process_pids[i], &status, WNOHANG);
            if (result > 0) {
                process_finished = true;
                if (WIFEXITED(status)) {
                    if (WEXITSTATUS(status) == 0) {
                        p[i].finished = true;
                        p[i].error = false;
                    } else {
                        p[i].finished = false;
                        p[i].error = true;
                    }
                } else {
                    p[i].finished = false;
                    p[i].error = true;
                }
                elapsed_time = get_current_time_ms() - start_execution;
                break;
            } else if (result < 0) {
                p[i].finished = false;
                p[i].error = true;
                process_finished = true;
                elapsed_time = get_current_time_ms() - start_execution;
                break;
            }
            elapsed_time = get_current_time_ms() - start_execution;
        }

        uint64_t time_spent = elapsed_time;
        p[i].burst_time += time_spent;

        current_time += elapsed_time;

        uint64_t process_end_time = get_current_time_ms() - start_time;
        print_context_switch(p[i].command, process_start_time, process_end_time);

        // If process didn't finish, stop it and requeue
        if (!process_finished) {
            kill(process_pids[i], SIGSTOP);
            enqueue(ready_queue, i);
            p[i].last_executed_time = current_time;
        } else {
            p[i].completion_time = current_time;
            p[i].turnaround_time = p[i].completion_time;
            p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;
            completed++;
        }
    }

    free_queue(ready_queue);
    free(process_pids);
    write_results_to_csv(p, n, "result_offline_RR.csv");
}

// Multi-Level Feedback Queue (MLFQ) scheduling algorithm
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime) {
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    int *current_queue = (int *)calloc(n, sizeof(int));
    uint64_t last_boost_time = 0;

    // Create three priority queues
    Queue *queues[3];
    for (int i = 0; i < 3; i++) {
        queues[i] = create_queue();
    }

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));
    
    // Initialize process data and queue
    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].burst_time = 0;
        p[i].waiting_time = 0;
        current_queue[i] = 0;
        enqueue(queues[0], i);
        process_pids[i] = -1;
    }

    while (completed < n) {
        // Check if it's time to boost all processes to the highest priority queue
        if (current_time - last_boost_time >= boostTime) {
            // Move all unfinished processes to the highest priority queue (queue 0)
            for (int i = 0; i < n; i++) {
                if (!p[i].finished) {
                    current_queue[i] = 0;  // Reset process queue to the highest priority
                    enqueue(queues[0], i);  // Add process to the highest priority queue
                }
            }
            last_boost_time = current_time;  // Update the time of the last boost
        }

        // Process each queue in order, from the highest priority (queue 0) to the lowest (queue 2)
        for (int queue = 0; queue < 3 && completed < n; queue++) {
            // Determine the quantum time for the current queue
            int quantum = (queue == 0) ? quantum0 : (queue == 1) ? quantum1 : quantum2;

            // Process all processes in the current queue
            while (queues[queue]->front != NULL) {
                int i = dequeue(queues[queue]);  // Get the process at the front of the queue
                if (i == -1 || p[i].finished) continue;  // Skip if invalid or process is finished

                // If the process has not started, record its start time and response time
                if (!p[i].started) {
                    p[i].start_time = current_time;
                    p[i].response_time = current_time;
                    p[i].started = true;
                }

                // Get the current time relative to when the scheduler started
                uint64_t process_start_time = get_current_time_ms() - start_time;

                // Fork or continue the process
                if (process_pids[i] == -1) {
                    // Fork a new process if it hasn't been created yet
                    process_pids[i] = fork();
                    if (process_pids[i] == 0) {
                        execute_command(p[i].command);  // Child process executes the command
                    } else if (process_pids[i] < 0) {
                        // If the fork failed, mark the process as finished with an error
                        p[i].finished = true;
                        p[i].error = true;
                        completed++;
                        continue;
                    }
                } else {
                    // Resume the suspended process
                    kill(process_pids[i], SIGCONT);
                }

                int status;
                uint64_t start_execution = get_current_time_ms();  // Start measuring execution time
                uint64_t elapsed_time = 0;
                bool process_finished = false;

                // Allow the process to execute for a duration up to the quantum
                while (elapsed_time < quantum) {
                    // Check if the process has finished
                    pid_t result = waitpid(process_pids[i], &status, WNOHANG);
                    if (result > 0) {
                        process_finished = true;  // Process finished successfully
                        if (WIFEXITED(status)) {
                            if (WEXITSTATUS(status) == 0) {
                                p[i].finished = true;  // Process completed without error
                                p[i].error = false;
                            } else {
                                p[i].finished = false;  // Process finished with an error
                                p[i].error = true;
                            }
                        } else {
                            p[i].finished = false;  // Process ended abnormally
                            p[i].error = true;
                        }
                        elapsed_time = get_current_time_ms() - start_execution;
                        break;
                    } else if (result < 0) {
                        // Error occurred while waiting for the process to finish
                        p[i].finished = false;
                        p[i].error = true;
                        process_finished = true;
                        elapsed_time = get_current_time_ms() - start_execution;
                        break;
                    }
                    elapsed_time = get_current_time_ms() - start_execution;  // Update elapsed time
                }

                p[i].burst_time += elapsed_time;  // Update burst time for the process
                current_time += elapsed_time;  // Update current time

                // Record the time the process finished or was suspended
                uint64_t process_end_time = get_current_time_ms() - start_time;
                print_context_switch(p[i].command, process_start_time, process_end_time);  // Print context switch details

                // If the process has not finished, suspend it and move it to the next queue
                if (!process_finished) {
                    kill(process_pids[i], SIGSTOP);  // Suspend the process
                    if (current_queue[i] < 2) {
                        current_queue[i]++;  // Move to the next lower priority queue
                    }
                    enqueue(queues[current_queue[i]], i);  // Add the process back to the queue
                } else {
                    // Process has finished; update its completion and turnaround times
                    p[i].completion_time = current_time;
                    p[i].turnaround_time = p[i].completion_time;
                    p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;
                    completed++;
                }
            }
        }
    }

    // Free the memory allocated for the queues and other structures
    for (int i = 0; i < 3; i++) {
        free_queue(queues[i]);  // Free each queue
    }
    free(current_queue);  // Free the queue tracker
    free(process_pids);   // Free the process IDs array

    // Write the results to a CSV file
    write_results_to_csv(p, n, "result_offline_MLFQ.csv");
}
