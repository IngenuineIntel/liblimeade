// diag.c
// AGPL

#include<stdio.h>

#include<liblimeade/liblimeade-internal.h>

void limeade_perror(const char * const s, const int error)
{
  fprintf(stderr, "%s: %s\n", s, LIMEADE_ERROR_REPRS[error]);
}

