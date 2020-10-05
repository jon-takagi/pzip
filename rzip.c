/*
 * A simple run-length encoder in C
 * The resulting file format is <length> (32-bit unsigned binary) followed
 * by a literal (b-bit character), repeated for as long as necessary.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////
// Given an input file pointer, read and compress the contents of the file to
// stdout.
void zip(FILE *fp)
{
  int c, last = fgetc(fp);
  uint32_t count;

  while (last != EOF) {
    for (count = 1; (c = fgetc(fp)) == last; count++)  // Count repeats of last
      ;

    if (fwrite(&count, sizeof(count), 1, stdout) < 1) {
      perror("Can't write to stdout");
      exit(1);
    }
    printf("%c", last);
    last = c;
  }

  if (!feof(fp)) {
    perror("Can't read from file");
    exit(1);
  }
}

//////////////////////////////////////////////////////////////////////////////
// Read all files listed in the argument list after a search term,
// and output matching lines to standard output.
int main(int argc, char** argv)
{
  if (argc <= 1) {
    fprintf(stderr, "%s: searchterm file ...\n", argv[0]);
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    FILE *in = fopen(argv[i], "r");
    if (!in) {
      perror("wunzip: cannot open file");
      exit(1);
    }
    zip(in);
    fclose(in);
  }

  return 0;
}
