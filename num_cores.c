//
// Created by ezha210 on 5/09/2018.
//

#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
    int processors = (int) sysconf(_SC_NPROCESSORS_CONF);
    printf("This machine has %d cores.", processors);
    return 0;
}