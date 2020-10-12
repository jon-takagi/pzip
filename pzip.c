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
    fp = fopen(file_name, "r");
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return size;
}



/*  function: compressor_thread
**    input (via void-casted pointer to a compthread_args struct):
**      worker_id: an integer indicating which segment of the target file this
**        compressor_thread is responsible for encoding.  The id is 0-indexed,
**        so the worker responsible for the 1st segment of the target file has
**        a worker_id of 0, the worker responsible for the 2nd segment has id 1,
**        and so on.
**      work_orders_for_file: an array of long integers, which are offsets (in
**        bytes) into the target file (from the start, at 0 bytes offset).
**        These indicate which segments of the target file each
**        compressor_thread is responsible for encoding.  In general, the thread
**        with worker_id i is responsible for the bytes starting after the block
**        of like characters at offset work_orders_for_file[i], up until the
**        first byte after the block of like characters at offset
**        work_orders_for_file[i+1].  The exceptions are the compressor_thread
**        with worker_id 0, which is responsible for the first segment of the
**        target file and thus is responsible for the bytes starting at offset
**        work_orders_for_file[0] rather than the bytes after the indicated
**        block, and the compressor_thread with worker_id num_jobs-1, which is
**        responsible for the last segment of the target file and is thus
**        responsible for the bytes beginning at the block after
**        work_orders_for_file[num_jobs-1] up until the end of the file.
**      num_jobs: an integer, indicating the total number of compressor_threads
**        working on the given target file.
**      target_file_name: a c-string, giving the name of the target file that
**        this this compressor_thread is being directed to compress.
**      completion_indicator: a pointer to a semaphore, which the
**        compressor_thread uses to the main thread that it's done encoding its
**        portion of the target file.
**
**    output (via void-casted pointer to a compthread_return struct):
**      num_entries: an unsigned integer, indicating the total number of like-
**        character blocks that were found in the bytes encoded by this
**        compressor_thread.
**      buffer: a character array, which stores the bytes that need to be sent
**        to stdout as a result of the work done by this compressor_thread.
**
**  Each compressor_thread starts by opening its own pointer to the target file,
**    and setting its offset to the position indicated by the work-order
**    corresponding to its id.  Then, if it's not the thread responsible for
**    encoding the first segment of the file, it scans forward in the file until
**    it finds a change in characters (so that it can guarantee that it's not
**    encoding part of a run of characters that the previous thread is
**    responsible for).
**  Once it finishes setting up, it performs run-length encoding, storing the
**    results in a heap-allocated buffer in the format in which they will be
**    sent to stdout (one 4-byte integer encoding followed by a 1-byte character
**    encoding for each run of like characters).  The buffer starts at 5 bytes,
**    and is doubled whenever it gets filled up, to try to balance the cost of
**    allocating large amounts of unnecesary memory vs the cost of performing
**    many memory allocations.
**  After it reaches the end of the segment of the target file its responsible
**    for, the compressor_thread closes its file pointer, creates a
**    compthread_return struct on the heap to hold its return values, and
**    returns a void-casted pointer to the struct (available to the main thread
**    via joining).  The main thread is then responsible for freeing the space
**    used by both the returned buffer and the compthread_return struct.
*/

struct compthread_args {
  int worker_id;
  long* work_orders_for_file;
  int num_jobs;
  char* target_file_name;
  sem_t* completion_indicator;
};

struct compthread_return {
  unsigned num_entries;
  char* buffer;
};

void* compressor_thread(void* arg) {
    //cast arg for later use
    struct compthread_args* args = (struct compthread_args*) arg;

    //open a stream to the target file
    FILE *fp;
    const char* mode = "r";
    fp = fopen(args->target_file_name, mode);
    assert(fp != NULL);

    //seek to the position in the file indicated by our work-order
    int rc = fseek(fp, args->work_orders_for_file[args->worker_id], SEEK_SET);
    assert(rc == 0);

    // get the first character at the work order
    char curr_char = (char) fgetc(fp);
    char new_char;

    // if we're not the first worker, scan forward in the file to get the
    //  first character we're actually responsible for.
    if (args->worker_id != 0) {
      for (new_char = fgetc(fp); new_char == curr_char; new_char = fgetc(fp));
      curr_char = new_char;
    }

    // set up the buffer where we'll store our output (in the format in which
    //  it needs to be sent to stdout)
    size_t buffer_size = 5;
    unsigned num_entries = 0;
    char* buffer = (char*) calloc(buffer_size, sizeof(char));

    uint32_t curr_char_count;

    while (1) { // loop until:

        // 1. We hit EOF
        if (curr_char == EOF) {
          break;
        }

        // 2. we hit the first character the next worker is responsible for
        //  (only if we're not the last worker)
        if ((args->worker_id != args->num_jobs-1) &&
              (ftell(fp) >= args->work_orders_for_file[args->worker_id+1])) { // if any other thread reaches the block that the next thread will start at
          break;
        }

        // count how many characters are in the current run
        for (curr_char_count = 1; (new_char = fgetc(fp)) == curr_char; curr_char_count++);

        // if necessary, double the size of the output buffer
        if (buffer_size <= num_entries * 5) {
            buffer_size = buffer_size * 2;
            buffer = (char*) realloc(buffer, buffer_size);
        }

        // store the information we gathered on the current run in the output
        //  buffer
        buffer[num_entries*5]   = ((char*) &curr_char_count)[0];
        buffer[num_entries*5+1] = ((char*) &curr_char_count)[1];
        buffer[num_entries*5+2] = ((char*) &curr_char_count)[2];
        buffer[num_entries*5+3] = ((char*) &curr_char_count)[3];
        buffer[num_entries*5+4] = curr_char;
        num_entries += 1;
        curr_char = new_char;
    }
    fclose(fp);

    // create the compthread_return struct we use to store our output on the
    //  heap, and set its values to what we need to return.
    struct compthread_return* rvals = calloc(1, sizeof(struct compthread_return));
    rvals->num_entries = num_entries; rvals->buffer = buffer;

    // tell main that we're done, and exit.
    sem_post(args->completion_indicator);
    return (void*) rvals;
}

/*  function: main
**    inputs (via command line):
**      A series of file names, indicating which files pzip should compress.
**    output (via stdout):
**      A series of bytes which is the run-length encoding of the input files.
**      Each input file is encoded seperately, but the results are concatenated
**      together.  Thus, if one input file ends with the same character that the
**      next input file begins with, both characters will be encoded as though
**      they were different characters.
**      Each run of like characters is encoded in 5 bytes.  The first 4 bytes
**      correspond to the bytes of a 32-bit unsigned integer giving the length
**      of the run.  The last byte corresponds to the character repeated in the
**      run.
**
**  main begins by getting the environment variable NTHREADS, which indicates
**    the number of compressor_threads that can be run in parallel during the
**    compression process, defaulting to 1 if NTHREADS is undefined.
**  Next, main initializes an array of the compthread_args structs used for
**    each compressor_thread that will used during the compression process.
**    Each input file is assigned NTHREADS compressor_threads, each of which
**    is responsible for an even portion of that file's bytes.
**  Next, main sets up a pthread array, containing a pthread object for each
**    compressor_thread that will be run (so that it can be joined, giving
**    main access to its output).
**  Once main is done initializing, it starts running compressor_threads.  As
**    long as there are compressor_threads yet to be run, it runs the next
**    compressor_thread scheduled to be run and then checks to see if the next
**    thread whose output it needs to write has finished running.  If it has,
**    main writes the output of that thread to stdout, and then frees the memory
**    used to return the output.  If it hasn't finished, main loops back and
**    waits until less than NTHREADS compressor_threads are running (using a
**    semaphore initalized to NTRHEADS, which main waits on before starting a
**    new compressor_thread and which each compressor_thread posts on
**    immediately before returning).
**  After all compressor_threads that will need to be started by main have been
**    started, main waits until each thread whose output it still needs to write
**    finishes in turn, writing the output to stdout and then freeing the memory
**    used to return it.
**  Once main finishes writing all output, it frees all the remaining memory
**    that needs to be freed, and exits.
*/

int main(int argc, char** argv) {

    // nthreads defaults to 1 if the NTHREADS environment variable is not defined
    char *nthreads_s = getenv("NTHREADS");
    long nthreads_l;
    if(nthreads_s == NULL) {
        nthreads_l = 1;
    } else {
        nthreads_l = atoi(nthreads_s);
    }

    // available_threads is a semaphore to control the number of simultaneously running threads
    sem_t available_threads;
    sem_init(&available_threads, 0, (unsigned int) nthreads_l);

    // all_thread_args is an array of the arguments for compressor_thread that
    //  will be run in the course of compressing the input files.  Each file
    //  will be compressed by nthreads_l compressor_threads, in an attempt to
    //  ensure full resource utilization.
    struct compthread_args* all_thread_args = (struct compthread_args*) calloc((argc-1)*nthreads_l, sizeof(struct compthread_args));

    // initalizing all_thread_args.  The compthread_args structs for each file
    //  share a work_orders_for_file array (allocated on the heap) and a
    //  target_file_name (stored in argv) - the rest of the data is duplicated.

    // looping over each input file:
    for(int j = 1; j < argc; j++) {

        // setting up the work_orders array for that file
        long size_of_file = file_size(argv[j]);
        long* work_orders = calloc(nthreads_l, sizeof(long));
        long work = 0;
        for (int i = 0; i < (int) nthreads_l; i++) {
          work_orders[i] = work;
          work = work + size_of_file / nthreads_l;
        }

        //looping over the compthread_args structs for the current file:
        for (int i = 0; i < nthreads_l; i++) {
          all_thread_args[(j-1) * nthreads_l + i].worker_id = i;
          all_thread_args[(j-1) * nthreads_l + i].work_orders_for_file = work_orders;
          all_thread_args[(j-1) * nthreads_l + i].num_jobs = (int) nthreads_l;
          all_thread_args[(j-1) * nthreads_l + i].target_file_name = argv[j];
          all_thread_args[(j-1) * nthreads_l + i].completion_indicator = &available_threads;
        }
    }

    // threads is an array of pthread_t objects, stored so that they can be
    //  joined by main when it's time to write their output to stdout.
    // output_head indicates the next thread in threads whose output needs to
    //  be written.
    // next_thread indicates the next thread in threads that main should start,
    //  when there are less than NTHREADS compressor_threads running.
    pthread_t* threads = (pthread_t*) calloc((argc-1) * nthreads_l, sizeof(pthread_t));
    int output_head = 0;
    int next_thread = 0;

    //as long as more threads need to be run:
    //  1. wait for the space to run a new thread to become available
    //  2. start a new thread running
    //  3. try to join the next thread whose ouput needs to be written.  If the
    //    join is successful, write its output, then free the memory used to
    //    return it.
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

    //After all threads have been started, while there are still threads whose
    //  output hasn't been written:
    //  1. join the next thread whose output needs to be written
    //  2. write its output, then free the memory used to return it.
    while (output_head < (argc - 1) * nthreads_l) {
      void* void_retval;
      pthread_join(threads[output_head], &void_retval);
      struct compthread_return* retval = (struct compthread_return*) void_retval;
      fwrite((void*) retval->buffer, 1, retval->num_entries*5, stdout);
      free(retval->buffer); free(retval);
      output_head++;
    }

    // free the memory used to store the thread arguments, then destroy the semaphore
    free(threads);
    for(int j = 1; j < argc; j++) {
      free(all_thread_args[(j-1) * nthreads_l].work_orders_for_file);
    }
    free(all_thread_args);
    sem_destroy(&available_threads);

    // exit in triumph, knowing that you have performed your duty with honor.
    return 0;
}
