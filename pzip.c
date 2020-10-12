#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>


// function:    file_size
//     input:   file name as a c string
//     output:  file size in bytes as an int
long file_size(char *file_name){
    FILE *fp;
    // printf("opening file %s\n", file_name);
    fp = fopen(file_name, "r");
    // printf("seeking to end of file %s\n", file_name);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    // printf("closing file %s\n", file_name);
    return size;
}


// function: compressor_thread

/* function - compressor_thread
**
**
*/

// main: create thread calling compress_file, passing the file name for each input
// set up ^ so that work_orders[i] is the 'start' of the work for thread i
// thread i compresses from byte i to byte work_orders[i+1], then finishes the block of identical characters that contains the char at work_orders[i+1]


struct compthread_args {
  int worker_id;            // starts at 0
  long* work_orders_for_file;        // a pointer to the work_orders for target_file_name
  int num_jobs;             // total # of threads, so each thread can tell if its the last one
  char* target_file_name;   // the name of the file to compress
  sem_t* completion_indicator;
};

struct compthread_return {
  unsigned num_entries;
  char* buffer;
};
//  expects arg to be a pointer to a valid compthread_arg struct (?)
//  we need id, the list of job sizes (but that can be public), and the file name (can also be public)
//  so all we really need, strictly speaking, is the id

// but we should probably pass arguments by struct anyway

//TODO: add to README that we're assuming this runs on an SSD (parallel reads)

void* compressor_thread(void* arg) {
    struct compthread_args* args = (struct compthread_args*) arg;
    FILE *fp;
    const char* mode = "r";
    // printf("Worker %d is opening file: %s\n", args->worker_id, args->target_file_name);

    fp = fopen(args->target_file_name, mode);
    assert(fp != NULL);

    int rc = fseek(fp, args->work_orders_for_file[args->worker_id], SEEK_SET);
    assert(rc == 0);

    char curr_char = (char) fgetc(fp);
    char new_char;

    if (args->worker_id != 0) { // do the scan-forward thing
      // printf("Scanning forward to the end of the block\n");
      for (new_char = fgetc(fp); new_char == curr_char; new_char = fgetc(fp));
      curr_char = new_char;
    }

    size_t buffer_size = 5;
    unsigned num_entries = 0;
    char* buffer = (char*) calloc(buffer_size, sizeof(char));

    uint32_t curr_char_count;

    // printf("beginning compresson loop\n");
    while ((curr_char != EOF)) {
        // ((args->worker_id != args->num_jobs-1) && (ftell(fp) > args->work_orders_for_file[args->worker_id+1])) ||
        // stop compression loop if:
        // if we are the last worker
            // we hit an EOF
        // if we are NOT the last worker
            // we have entered the block where the next guy would start
        for (curr_char_count = 1; (new_char = fgetc(fp)) == curr_char; curr_char_count++);

        // if the buffer is too small, expand it
        if (buffer_size <= num_entries * 5) {
            buffer_size = buffer_size * 2;
            buffer = (char*) realloc(buffer, buffer_size);
        }

        buffer[num_entries*5]   = ((char*) &curr_char_count)[0];
        buffer[num_entries*5+1] = ((char*) &curr_char_count)[1];
        buffer[num_entries*5+2] = ((char*) &curr_char_count)[2];
        buffer[num_entries*5+3] = ((char*) &curr_char_count)[3];
        
        buffer[num_entries*5+4] = curr_char;
        num_entries += 1;
        curr_char = new_char;
    }
    fclose(fp);

    struct compthread_return* rvals = calloc(1, sizeof(struct compthread_return));
    rvals->num_entries = num_entries; rvals->buffer = buffer;

    sem_post(args->completion_indicator);
    return (void*) rvals;
}
/*
// printing this should be
struct compressor_return {
  long long length;
  uint32* counts;
  char* chars;
}*/

//Compressor-thread returns (via join) a void-casted pointer to (? thing you're supposed to write)

// we get an input AND output thread?!?! Then we should definitely use an output thread. . .
// think about how to make that work . . . the issue is still that thread 1 has to complete

int main(int argc, char** argv) {
    // int NUM_CORES = get_nprocs_conf();
    // printf("This system has %d processors configured and %d processors available.\n", NUM_CORES, get_nprocs());
    char *nthreads_s = getenv("NTHREADS");
    long nthreads_l;
    if(nthreads_s == NULL) {
        nthreads_l = 1;
    } else {
        nthreads_l = atoi(nthreads_s);
    }
    // printf("NTHREADS: %ld\n", nthreads_l);

    sem_t available_threads;
    sem_init(&available_threads, 0, (unsigned int) nthreads_l);

    struct compthread_args* all_thread_args = (struct compthread_args*) calloc((argc-1)*nthreads_l, sizeof(struct compthread_args));
    // printf("created args array\n");
    for(int j = 1; j < argc; j++) {
        long size_of_file = file_size(argv[j]);
        long* work_orders = calloc(nthreads_l, sizeof(long));
        long work = 0;
        for (int i = 0; i < (int) nthreads_l; i++) {
          work_orders[i] = work;
          work = work + size_of_file / nthreads_l;
        }
        for (int i = 0; i < nthreads_l; i++) {
          all_thread_args[(j-1) * nthreads_l + i].worker_id = i;
          all_thread_args[(j-1) * nthreads_l + i].work_orders_for_file = work_orders;
          all_thread_args[(j-1) * nthreads_l + i].num_jobs = (int) nthreads_l;
          all_thread_args[(j-1) * nthreads_l + i].target_file_name = argv[j];
          all_thread_args[(j-1) * nthreads_l + i].completion_indicator = &available_threads;
        }
    }
    // printf("initialized args\n");
    pthread_t* threads = (pthread_t*) calloc((argc-1) * nthreads_l, sizeof(pthread_t));
    int output_head = 0;
    int next_thread = 0; // index into all_thread_args. points to the args for the next thread being created
    while (next_thread < (argc-1) * nthreads_l) {
        sem_wait(&available_threads);
        pthread_create(&threads[next_thread], NULL, compressor_thread, (void*) &all_thread_args[next_thread]);
        next_thread++;
        void* void_retval;
        if (pthread_tryjoin_np(threads[output_head], &void_retval) == 0) {
            struct compthread_return* retval = (struct compthread_return*) void_retval;
            fwrite((void*) retval->buffer, 5, retval->num_entries, stdout);
            free(retval->buffer); free(retval);
            output_head++;
        }
    }
    while (output_head < (argc - 1) * nthreads_l) {
      // printf("joining thread %d\n", output_head);
      void* void_retval;
      pthread_join(threads[output_head], &void_retval);
      // printf("building retval\n");
      struct compthread_return* retval = (struct compthread_return*) void_retval; //TODO: maybe this should be a function?
      // printf("done\n");
      // printf("number of entries reported: %u\n", retval->num_entries);
      fwrite((void*) retval->buffer, 1, retval->num_entries*5, stdout);

      free(retval->buffer); free(retval);
      output_head++;
    }
    free(threads);
    for(int j = 1; j < argc; j++) {
      free(all_thread_args[(j-1) * nthreads_l].work_orders_for_file);
    }
    free(all_thread_args);
    sem_destroy(&available_threads);
    return 0;
}
