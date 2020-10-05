#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
void* read_from_file(void* arg) {
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
    printf("NTHREADS: %s\n", getenv("NTHREADS"));
    char *nthreads_s = getenv("NTHREADS");
    int threads;
    if(nthreads_s == NULL) {
        threads = 1;
    } else {
        threads = atoi(nthreads_s);
    }
    printf("%d\n", threads);
    char fname[] = {'h','e','l','l','o','.','t','x','t','\0'};
    void *argptr = (void* )fname;
    pthread_t t1;
    pthread_t t2;
    pthread_create(&t1, NULL, read_from_file, argptr);
    pthread_create(&t2, NULL, read_from_file, argptr);
    void *retval1;
    void *retval2;
    pthread_join(t1, (void **) &retval1);
    pthread_join(t2, (void **) &retval2);
    // retval is the value returned by the thread - (void *) 0
    printf("thread 1 retval: %lld\n", (long long int)retval1);
    printf("thread 1 retval: %lld\n", (long long int)retval2);
    return 0;
}
