
#include <stdlib.h> 
#include <unistd.h> 
#include <stdio.h>
#include "helper.h"

int main(int argc, char** argv)
{
    if(argc != 2){return 1;}
    int timeInSec = atoi(argv[1]);
    uint64_t start = current_time(0);
    while(current_time(0) - start < timeInSec * 1000);
}