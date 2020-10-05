#include <stdio.h>
#include <sys/sysinfo.h>

int main(int argc, char** argv) {
    int NUM_CORES = get_nprocs_conf();
    printf("This system has %d processors configured and %d processors available.\n", NUM_CORES, get_nprocs());
    return 0;
}
