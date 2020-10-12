## Parallel ZIP
#### Jon Takagi and Robert McCaull
This solution is based on Eitan's rzip, also included in this code.

Our solution follows Eitan's solution (rather than the book's) behavior when multiple files are passed.
When Eitan's rzip is passed multiple files, it processes each file individually, then appends it to the result of the previous file. The book's solution expects that the input files are concatenated together then processed.
As an example, consider the file containing "AABBB" and the file "BBCC". While the book would expect 2A5B2C (with binary numbers), our code would output "2A3B2B2C".

Furthermore, our solution assumes hardware support for parallel reads from storage. While this assumption holds for solid state disks, it is not true of hard disk drives.

#### `main`
Main begins by creating `all_thread_args`. Each element of the array is a struct containing the arguments for a worker thread - its worker ID, the file it will process, its offset inside that file, whether it runs to the end of file or to the start of the next thread, and a completion indicator.

During the first phase of execution, a semaphore is used to ensure that the maximum allowed number of threads are running concurrently. If main sees that there is room for another thread, it starts one.

Once the final set of threads has been created, main waits for them to finish using `pthread_join`. As they return, main writes their outputs to stdout.

Finally, all our memory is free'd. Overall, this program uses a lot of heap memory, but it's all released at the end.

#### `compressor_thread`
The compressor thread opens its specified file, and scrolls to its specified starting location. It then finds the first new sequence of characters beginning after its specified start location. It then works from there up to the start location of the next specified thread, completing its sequence.
