#include "../../offline_schedulers.h"

int main(){
    int n = 2;
    Process p[n];
    p[0].command = "./dummy_p 3";
    p[1].command = "./dummy_p 1";
    RR(p, n, 1000);
}