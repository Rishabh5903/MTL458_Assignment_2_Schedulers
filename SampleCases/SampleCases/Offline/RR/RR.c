#include "E:\IIT DELHI\OneDrive - IIT Delhi\IIT DELHI\sem 7\Courses\MTL458\assign\Assingment 2\offline_schedulers.h"

int main(){
    int n = 3;
    Process p[n];
    p[0].command = "./dummy_p 3";
    p[1].command = "bhth";

    p[2].command = "./dummy_p 1";
    RoundRobin(p, n, 1000);
}