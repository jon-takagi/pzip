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
struct compthread_args {
  int worker_id; // starts at 0
  long* job_sizes;
  int num_jobs;
  char* target_file_name;
}
// expects arg to be a pointer to a valid compthread_arg struct (?)
//  we need id, the list of job sizes (but that can be public), and the file name (can also be public)
//  so all we really need, strictly speaking, is the id

// but we should probably pass arguments by struct anyway

//TODO: add to README that we're assuming this runs on an SSD (parallel reads)

void* compressor_thread(void* arg) {
    struct compthread_args* args = (compthread_arg*) arg;
    FILE *fp;
    const char* mode = "r";
    printf("Worker %d is opening file: %s\n", args->worker_id, arg_s);
    fp = fopen(args->target_file_name, mode); //TODO: check if this worked!

    fseek(fp, args->job_sizes[args->worker_id], SEEK_SET); //TODO: check if this worked!

    char curr_char = fgetc(fp);
    long curr_char_count = 1;
    if (args->worker_id != 0) { // do the scan-forward thing
      char new_char = fgetc(fp);
      while (new_char == curr_char) {
        curr_char_count = curr_char_count + 1;
      }
      curr_char = new_char;
      curr_char_count = 1;
    }

  while(true) {
    if ((args->worker_id != args->num_jobs-1) && (ftell(fp) > args->job_sizes[args->worker_id])
        || (curr_char == EOF)) {
      break;
    }
    for (count = 1; (new_char = fgetc(fp)) == curr_char; count++);
    //add to the to-write struct
  }
  fclose(fp);
  return (void *) retval;
}

// printing this should be
struct compressor_return {
  long long length;
  uint32* counts;
  char* chars;
}

//Compressor-thread returns (via join) a void-casted pointer to (? thing you're supposed to write)

// we get an input AND output thread?!?! Then we should definitely use an output thread. . .
// think about how to make that work . . . the issue is still that thread 1 has to complete

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
    int work = 0;
    for (int i = 0; i < sizeof(file_sizes) / sizeof(int); i++) {
        work += file_sizes[i];
    }
    // work_orders[i] is the start byte for the i'th thread
    //
    int jobsize = work / threads;
    for (int i = 1; i < threads; i++) {
      work_orders[i] = work_orders[i-1] + jobsize;
    }

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
