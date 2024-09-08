#include "online_schedulers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_menu() {
    printf("\nOnline Scheduler Test Program\n");
    printf("1. Shortest Job First (SJF)\n");
    printf("2. Multi-level Feedback Queue (MLFQ)\n");
    printf("3. Exit\n");
    printf("Enter your choice: ");
}

int main() {
    int choice;
    int quantum0 = 200, quantum1 = 300, quantum2 = 500, boostTime = 1000;

    while (1) {
        print_menu();
        scanf("%d", &choice);
        getchar(); // Consume newline

        switch (choice) {
            case 1:
                printf("Running Shortest Job First (SJF) Scheduler\n");
                printf("Enter commands, one per line. Enter an empty line to finish input.\n");
                ShortestJobFirst();
                break;

            case 2:
                printf("Running Multi-level Feedback Queue (MLFQ) Scheduler\n");
                printf("Enter commands, one per line. Enter an empty line to finish input.\n");
                MultiLevelFeedbackQueue(quantum0, quantum1, quantum2, boostTime);
                break;

            case 3:
                printf("Exiting program.\n");
                exit(0);

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}