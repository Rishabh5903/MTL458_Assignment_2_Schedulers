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

void execute_command(const char* command) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull == -1) {
        exit(1);
    }
    
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    char *args[] = {"/bin/sh", "-c", (char*)command, NULL};
    execvp(args[0], args);
    exit(1);
}

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

void print_context_switch(const char* command, uint64_t start_time, uint64_t end_time) {
    printf("%s|%lu|%lu\n", command, start_time, end_time);
}

void FCFS(Process p[], int n) {
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time = 0;

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

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

    for (int i = 0; i < n; i++) {
        p[i].start_time = current_time;
        p[i].response_time = p[i].start_time;
        p[i].started = true;

        uint64_t process_start_time = get_current_time_ms() - start_time;

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

        p[i].completion_time = execution_end_time - start_time;
        p[i].turnaround_time = p[i].completion_time;
        p[i].waiting_time = p[i].response_time;
        p[i].burst_time = execution_end_time - execution_start_time;

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





void RoundRobin(Process p[], int n, int quantum) {
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time = 0;
    int completed = 0;
    Queue *ready_queue = create_queue();

    pid_t *process_pids = (pid_t *)malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        p[i].started = false;
        p[i].finished = false;
        p[i].last_executed_time = 0;
        p[i].burst_time = 0;
        p[i].waiting_time = 0;
        process_pids[i] = -1;
        enqueue(ready_queue, i);
    }

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





void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime) {
    uint64_t start_time = get_current_time_ms();
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
        current_queue[i] = 0;
        enqueue(queues[0], i);
        process_pids[i] = -1;
    }

    while (completed < n) {
        if (current_time - last_boost_time >= boostTime) {
            for (int i = 0; i < n; i++) {
                if (!p[i].finished) {
                    current_queue[i] = 0;
                    enqueue(queues[0], i);
                }
            }
            last_boost_time = current_time;
        }

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

                uint64_t process_start_time = get_current_time_ms() - start_time;

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

                p[i].burst_time += elapsed_time;
                current_time += elapsed_time;

                uint64_t process_end_time = get_current_time_ms() - start_time;
                print_context_switch(p[i].command, process_start_time, process_end_time);

                if (!process_finished) {
                    kill(process_pids[i], SIGSTOP);
                    if (current_queue[i] < 2) {
                        current_queue[i]++;
                    }
                    enqueue(queues[current_queue[i]], i);
                } else {
                    p[i].completion_time = current_time;
                    p[i].turnaround_time = p[i].completion_time - p[i].start_time;
                    p[i].waiting_time = p[i].turnaround_time - p[i].burst_time;
                    completed++;
                }
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        free_queue(queues[i]);
    }
    free(current_queue);
    free(process_pids);
    write_results_to_csv(p, n, "result_offline_MLFQ.csv");
}