#include "online_schedulers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sleep()

void print_menu() {
    printf("\nOnline Scheduler Test Program\n");
    printf("1. Shortest Job First (SJF)\n");
    printf("2. Shortest Remaining Time First (SRTF)\n");
    printf("3. Multi-level Feedback Queue (MLFQ)\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

int check_new_input(ProcessList *process_list, HistoricalDataList *historical_data) {
    char command[MAX_COMMAND_LENGTH];
    printf("\nEnter any new commands (or empty line to continue scheduling):\n");

    // Check for new input
    while (fgets(command, MAX_COMMAND_LENGTH, stdin) != NULL && command[0] != '\n') {
        command[strcspn(command, "\n")] = 0; // Remove newline
        if (strlen(command) > 0) {
            add_process(process_list, command, historical_data);
        }
    }
    return process_list->count;
}

int main() {
    ProcessList process_list = {0};
    HistoricalDataList historical_data = {0};

    int choice;
    int quantum0, quantum1, quantum2, boostTime;

    while (1) {
        print_menu();
        scanf("%d", &choice);
        getchar(); // Consume newline

        switch (choice) {
            case 1:
                printf("Running Shortest Job First (SJF) Scheduler\n");
                while (1) {
                    // Run scheduler
                    ShortestJobFirst(&process_list);

                    // Check for real-time input (if no new input, we exit)
                    if (!check_new_input(&process_list, &historical_data)) {
                        break;
                    }
                }
                break;

            case 2:
                printf("Running Shortest Remaining Time First (SRTF) Scheduler\n");
                while (1) {
                    // Run scheduler
                    ShortestRemainingTimeFirst(&process_list);

                    // Check for real-time input
                    if (!check_new_input(&process_list, &historical_data)) {
                        break;
                    }
                }
                break;

            case 3:
                printf("Running Multi-level Feedback Queue (MLFQ) Scheduler\n");
                printf("Enter quantum for high priority queue: ");
                scanf("%d", &quantum0);
                printf("Enter quantum for medium priority queue: ");
                scanf("%d", &quantum1);
                printf("Enter quantum for low priority queue: ");
                scanf("%d", &quantum2);
                printf("Enter priority boost time: ");
                scanf("%d", &boostTime);
                getchar(); // Consume newline

                while (1) {
                    // Run MLFQ scheduler
                    MultiLevelFeedbackQueue(&process_list, quantum0, quantum1, quantum2, boostTime);

                    // Check for real-time input
                    if (!check_new_input(&process_list, &historical_data)) {
                        break;
                    }
                }
                break;

            case 4:
                printf("Exiting program.\n");
                exit(0);

            default:
                printf("Invalid choice. Please try again.\n");
        }

        // Reset process list for next run
        process_list.count = 0;
    }

    return 0;
}