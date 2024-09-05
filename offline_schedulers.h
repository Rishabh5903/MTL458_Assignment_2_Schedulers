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

uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

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


void write_results_to_csv(Process p[], int n, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // Add the header
    fprintf(fp, "Command,Finished,Error,Completion Time,Turnaround Time,Waiting Time,Response Time\n");

    for (int i = 0; i < n; i++) {
        // Enclose the command in double quotes to handle spaces and special characters
        fprintf(fp, "\"%s\",%s,%s,%lu,%lu,%lu,%lu\n",
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





void FCFS(Process p[], int n) {
    uint64_t start_time = get_current_time_ms();  // Scheduler start time
    uint64_t current_time = 0;

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].completion_time = 0;
        p[i].turnaround_time = 0;
        p[i].waiting_time = 0;
        p[i].response_time = 0;
        process_pids[i] = -1;
    }

    for (int i = 0; i < n; i++) {
        // Set process start time and response time
        p[i].start_time = current_time;  // Relative to global start (t=0)
        p[i].response_time = p[i].start_time;
        p[i].started = true;

        printf("Executing: %s (Start Time: %lu ms)\n", p[i].command, p[i].start_time);

        // Fork the process
        process_pids[i] = fork();
        if (process_pids[i] == 0) {
            // Child process
            char *args[] = {"/bin/sh", "-c", p[i].command, NULL};
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);  // Exit if execvp fails
        } else if (process_pids[i] < 0) {
            // Fork failed
            perror("fork failed");
            p[i].finished = true;
            p[i].error = true;
            continue;  // Move to the next process
        }

        // Parent process: wait for the child to complete
        int status;
        uint64_t execution_start_time = get_current_time_ms();
        waitpid(process_pids[i], &status, 0);  // Wait for process to complete
        uint64_t execution_end_time = get_current_time_ms();

        // Calculate process times
        p[i].completion_time = execution_end_time - start_time;  // Relative to t=0
        p[i].turnaround_time = p[i].completion_time - p[i].start_time;  // From start to completion
        p[i].waiting_time = p[i].turnaround_time;  // In FCFS, waiting time == turnaround time since there's no time slice
        p[i].burst_time = execution_end_time - execution_start_time;  // Time the process actually ran

        if (WIFEXITED(status)) {
            p[i].finished = true;
            p[i].error = WEXITSTATUS(status) != 0;
        } else {
            p[i].error = true;
        }

        printf("Process completed: %s\n", p[i].command);
        printf("Finished: %s\n", p[i].finished ? "Yes" : "No");
        printf("Error: %s\n", p[i].error ? "Yes" : "No");
        printf("Completion Time: %lu ms\n", p[i].completion_time);
        printf("Turnaround Time: %lu ms\n", p[i].turnaround_time);
        printf("Waiting Time: %lu ms\n", p[i].waiting_time);
        printf("Response Time: %lu ms\n\n", p[i].response_time);

        current_time = p[i].completion_time;  // Update current time for next process
    }

    free(process_pids);  // Clean up the allocated memory for PIDs
    write_results_to_csv(p, n, "result_offline_FCFS.csv");  // Write results to CSV
}







void RoundRobin(Process p[], int n, int quantum) {
    uint64_t current_time = 0;
    int completed = 0;
    Queue *ready_queue = create_queue();

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].last_executed_time = 0;  // Initialize the last execution time to 0
        p[i].burst_time = 0;          // Initialize burst time to 0
        p[i].waiting_time = 0;        // Initialize waiting time to 0
        process_pids[i] = -1;
        enqueue(ready_queue, i);
    }

    while (completed < n) {
        int i = dequeue(ready_queue);
        if (i == -1) break;  // No more processes in the queue

        if (p[i].finished) {
            continue;  // Skip already finished processes
        }

        if (!p[i].started) {
            p[i].start_time = current_time;
            p[i].response_time = current_time;  // Response time is set when the process first starts
            p[i].started = true;
            p[i].last_executed_time = current_time;  // Set last executed time to the start time
        }

        printf("Executing: %s (Time: %lu ms)\n", p[i].command, current_time);

        if (process_pids[i] == -1) {
            // Start a new process
            process_pids[i] = fork();
            if (process_pids[i] == 0) {
                // Child process
                char *args[] = {"/bin/sh", "-c", p[i].command, NULL};
                execvp(args[0], args);
                exit(1);  // Exit if execvp fails
            } else if (process_pids[i] < 0) {
                // Fork failed
                perror("fork failed");
                p[i].finished = true;
                p[i].error = true;
                completed++;
                continue;
            }
        } else {
            // Resume the process
            kill(process_pids[i], SIGCONT);
        }

        // Parent process
        int status;
        uint64_t start_time = get_current_time_ms();
        uint64_t elapsed_time = 0;
        bool process_finished = false;

        while (elapsed_time < quantum) {
            pid_t result = waitpid(process_pids[i], &status, WNOHANG);
            if (result > 0) {
                // Process finished
                process_finished = true;
                if (WIFEXITED(status)) {
                    p[i].finished = true;
                    p[i].error = WEXITSTATUS(status) != 0;
                } else {
                    p[i].finished = true;
                    p[i].error = true;
                }
                elapsed_time = get_current_time_ms() - start_time;
                break;
            } else if (result < 0) {
                // Error occurred
                perror("waitpid");
                p[i].finished = true;
                p[i].error = true;  // Mark error and terminate
                process_finished = true;
                elapsed_time = get_current_time_ms() - start_time;
                break;
            }
            elapsed_time = get_current_time_ms() - start_time;
        }

        // Calculate time spent running
        uint64_t time_spent = elapsed_time;
        p[i].burst_time += time_spent;  // Add the running time to the burst time

        current_time += elapsed_time;

        if (!process_finished) {
            // Time quantum expired, stop the process
            kill(process_pids[i], SIGSTOP);
            enqueue(ready_queue, i);  // Put it back in the queue
            p[i].last_executed_time = current_time;  // Update last executed time
        } else {
            // Process finished
            p[i].completion_time = current_time;
            p[i].turnaround_time = p[i].completion_time;

            // Calculate waiting time using the formula: Turnaround Time - Burst Time
            p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;

            printf("Process completed: %s\n", p[i].command);
            printf("Finished: %s\n", p[i].finished ? "Yes" : "No");
            printf("Error: %s\n", p[i].error ? "Yes" : "No");
            printf("Completion Time: %lu ms\n", p[i].completion_time);
            printf("Turnaround Time: %lu ms\n", p[i].turnaround_time);
            printf("Waiting Time: %lu ms\n", p[i].waiting_time);
            printf("Burst Time: %lu ms\n", p[i].burst_time);

            completed++;
        }
    }

    // Clean up the process queue
    free_queue(ready_queue);
}







void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime) {
    uint64_t current_time = 0;
    int completed = 0;
    int *current_queue = (int *)calloc(n, sizeof(int));
    uint64_t last_boost_time = 0;

    Queue *queues[3];
    for (int i = 0; i < 3; i++) {
        queues[i] = create_queue();
    }

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));
    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].burst_time = 0;
        p[i].waiting_time = 0;
        current_queue[i] = 0;  // Start all in the highest priority queue
        enqueue(queues[0], i);  // Add to highest priority queue initially
        process_pids[i] = -1;  // No process started yet
    }

    while (completed < n) {
        // Priority boost check
        if (current_time - last_boost_time >= boostTime) {
            for (int i = 0; i < n; i++) {
                if (!p[i].finished) {
                    current_queue[i] = 0;  // Move all processes back to Q0
                    enqueue(queues[0], i);
                }
            }
            last_boost_time = current_time;
        }

        // Iterate through each queue
        for (int queue = 0; queue < 3 && completed < n; queue++) {
            int quantum = (queue == 0) ? quantum0 : (queue == 1) ? quantum1 : quantum2;

            while (queues[queue]->front != NULL) {
                int i = dequeue(queues[queue]);
                if (i == -1 || p[i].finished) continue;

                if (!p[i].started) {
                    p[i].start_time = current_time;
                    p[i].response_time = current_time;
                    p[i].started = true;
                }

                printf("Executing: %s (Queue %d, Time: %lu ms)\n", p[i].command, queue, current_time);

                if (process_pids[i] == -1) {
                    // Start a new process
                    process_pids[i] = fork();
                    if (process_pids[i] == 0) {
                        // Child process
                        char *args[] = {"/bin/sh", "-c", p[i].command, NULL};
                        execvp(args[0], args);
                        exit(1);  // Exit if execvp fails
                    } else if (process_pids[i] < 0) {
                        // Fork failed
                        perror("fork failed");
                        p[i].finished = true;
                        p[i].error = true;
                        completed++;
                        continue;
                    }
                } else {
                    // Resume the process
                    kill(process_pids[i], SIGCONT);
                }

                // Parent process: Track execution time
                int status;
                uint64_t start_time = get_current_time_ms();
                uint64_t elapsed_time = 0;
                bool process_finished = false;

                while (elapsed_time < quantum) {
                    pid_t result = waitpid(process_pids[i], &status, WNOHANG);
                    if (result > 0) {
                        // Process finished
                        process_finished = true;
                        if (WIFEXITED(status)) {
                            p[i].finished = true;
                            p[i].error = WEXITSTATUS(status) != 0;
                        } else {
                            p[i].finished = true;
                            p[i].error = true;
                        }
                        elapsed_time = get_current_time_ms() - start_time;
                        break;
                    } else if (result < 0) {
                        // Error occurred
                        perror("waitpid");
                        p[i].finished = true;
                        p[i].error = true;
                        process_finished = true;
                        elapsed_time = get_current_time_ms() - start_time;
                        break;
                    }
                    elapsed_time = get_current_time_ms() - start_time;
                }

                // Update burst time and current time
                p[i].burst_time += elapsed_time;
                current_time += elapsed_time;

                if (!process_finished) {
                    // Time quantum expired, stop the process
                    kill(process_pids[i], SIGSTOP);
                    if (current_queue[i] < 2) {
                        current_queue[i]++;  // Move to lower priority queue
                    }
                    enqueue(queues[current_queue[i]], i);
                } else {
                    // Process completed
                    p[i].completion_time = current_time;
                    p[i].turnaround_time = p[i].completion_time - p[i].start_time;
                    p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;

                    printf("Process completed: %s\n", p[i].command);
                    printf("Finished: %s\n", p[i].finished ? "Yes" : "No");
                    printf("Error: %s\n", p[i].error ? "Yes" : "No");
                    printf("Completion Time: %lu ms\n", p[i].completion_time);
                    printf("Turnaround Time: %lu ms\n", p[i].turnaround_time);
                    printf("Waiting Time: %lu ms\n", p[i].waiting_time);
                    printf("Burst Time: %lu ms\n", p[i].burst_time);

                    completed++;
                }
            }
        }
    }

    // Clean up the resources
    for (int i = 0; i < 3; i++) {
        free_queue(queues[i]);
    }
    free(current_queue);
    free(process_pids);
    write_results_to_csv(p, n, "result_offline_MLFQ.csv");
}