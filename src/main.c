#define _GNU_SOURCE

#include "../include/api_assembler.h"
#include "../include/elf_emit.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* prog_name = "riscv-as";

static void usage(FILE* out) {
  fprintf(out,
          "Usage: %s [options] file\n"
          "Options:\n"
          "  -o <file>  Place the output into <file> (default: a.out)\n"
          "  -h         Display this information\n",
          prog_name);
}

static char* read_file(const char* path, size_t* len_out) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;

  char* buf = NULL;
  if (fseek(f, 0, SEEK_END) != 0)
    goto fail;
  long size = ftell(f);
  if (size < 0)
    goto fail;
  rewind(f);

  buf = malloc((size_t)size + 1);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size)
    goto fail;

  buf[size] = '\0';
  fclose(f);
  if (len_out)
    *len_out = (size_t)size;
  return buf;

fail:
  free(buf);
  fclose(f);
  return NULL;
}

int main(int argc, char** argv) {
  if (argc > 0 && argv[0])
    prog_name = argv[0];

  const char* output = "a.out";
  int opt;
  while ((opt = getopt(argc, argv, "o:h")) != -1) {
    switch (opt) {
    case 'o':
      output = optarg;
      break;
    case 'h':
      usage(stdout);
      return 0;
    default:
      usage(stderr);
      return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "%s: fatal error: no input files\n", prog_name);
    return 1;
  }
  if (argc - optind > 1) {
    fprintf(stderr, "%s: fatal error: only one input file is supported\n", prog_name);
    return 1;
  }

  const char* input = argv[optind];
  char* source = read_file(input, NULL);
  if (!source) {
    perror(input);
    return 1;
  }

  AssembledProgram* program = assemble(source);
  if (!program)
    return 1;

  errno = 0;
  int rc = write_elf_file(program, output);
  int err = errno;
  free_assembled_program(program);
  if (rc != 0) {
    if (err != 0)
      fprintf(stderr, "%s: error: failed to write '%s': %s\n", prog_name, output, strerror(err));
    else
      fprintf(stderr, "%s: error: failed to write '%s'\n", prog_name, output);
    return 1;
  }
  return 0;
}