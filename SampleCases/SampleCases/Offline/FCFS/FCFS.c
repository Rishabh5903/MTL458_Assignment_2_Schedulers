#include "../../offline_schedulers.h"

int main(){
    int n = 3;
    Process p[n];
    p[0].command = "./dummy_p 3";
    p[1].command = "./dummy_p 1";
    p[2].command = "command_to_throw_error";

    FCFS(p, n);
}