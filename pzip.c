#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>


// function: get_file_sizes
//     input: array of strings representing file names
//     output: heap-array of ints where input[0] = the size of the file in bytes
//       **  !!  MUST BE FREED BY CALLER  !!  **
int* get_file_sizes(char **args){
    int* heap_space = (int *) calloc(sizeof(args) / sizeof(char), sizeof(int));
    FILE *fp;
    for(int i = 0; i < sizeof(args) / sizeof(char); i++) {
        // printf("opening %s\n", args[i]);
        fp = fopen(args[i], "r");
        fseek(fp, 0, SEEK_END);
        heap_space[i] = ftell(fp);
        fclose(fp);
    }
    return heap_space;
}


// function: compressor_thread

/* function - compressor_thread
**
**
*/

// main: create thread calling compress_file, passing the file name for each input

long* work_orders;
long threads;
// set up ^ so that work_orders[i] is the 'start' of the work for thread i
// thread i reads from byte i to byte work_orders[i+1]
void* compressor_thread(void* arg) {
    long long int retval;
    FILE *fp;
    const char* mode = "r";
    char* arg_s = (char *) arg;
    printf("opening file: %s\n", arg_s);
    fp = fopen(arg_s, mode);
    char buff[127];
    if(fgets(buff, 127, fp)) {
        printf("content: %s\n", buff);
        retval = 0;
    } else {
        retval = 1;
    }
    fclose(fp);
    return (void *) retval;
}

int main(int argc, char** argv) {
    // int NUM_CORES = get_nprocs_conf();
    // printf("This system has %d processors configured and %d processors available.\n", NUM_CORES, get_nprocs());
    char *nthreads_s = getenv("NTHREADS");
    if(nthreads_s == NULL) {
        threads = 1;
    } else {
        threads = atoi(nthreads_s);
    }
    printf("NTHREADS: %ld\n", threads);
    work_orders = (long *)calloc(threads, sizeof(long));
    work_orders[0] = 0;
    int* file_sizes = get_file_sizes(argv);

    // TODO: come up with algorithm for assigning n threads to k files in some intelligent way
    // int jobsize = work / threads;
    // for (int i = 1; i < threads; i++) {
    //   work_orders[i] = work_orders[i-1] + jobsize;
    // }

    // char fname[] = {'h','e','l','l','o','.','t','x','t','\0'};
    // void *argptr = (void* )fname;
    // pthread_t t1;
    // pthread_t t2;
    // pthread_create(&t1, NULL, read_from_file, argptr);
    // pthread_create(&t2, NULL, read_from_file, argptr);
    // void *retval1;
    // void *retval2;
    // pthread_join(t1, (void **) &retval1);
    // pthread_join(t2, (void **) &retval2);
    // // retval is the value returned by the thread - (void *) 0
    // printf("thread 1 retval: %lld\n", (long long int)retval1);
    // printf("thread 1 retval: %lld\n", (long long int)retval2);

    free(file_sizes);
    return 0;
}
