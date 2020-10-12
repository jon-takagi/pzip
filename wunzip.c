/*
 * A simple run-length decoder in C
 * The input file format is <length> (32-bit unsigned binary) followed
 * by a literal (b-bit character), repeated for as long as necessary.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////
// Given an input file pointer, read and uncompress the contents of the file to
// stdout.
void unzip(FILE *fp)
{
  int c;
  uint32_t count;

  while (fread(&count, sizeof(count), 1, fp) == 1 && fread(&c, 1, 1, fp) == 1) {
    for (uint32_t i = 0; i < count; i++) {
      printf("%c", c);
    }
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
  if (argc < 2) {
    fprintf(stderr, "%s: searchterm file ...\n", argv[0]);
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    FILE *in = fopen(argv[i], "r");
    if (!in) {
      perror("wunzip: cannot open file");
      exit(1);
    }
    unzip(in);
    fclose(in);
  }

  return 0;
}

