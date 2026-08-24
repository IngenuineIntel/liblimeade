// diag.c

// AGPL

#include<liblimeade/liblimeade-internal.h>

void limeade_diag_repr_error(enum limeade_error e)
{
  printf("error: %s\n", LIMEADE_ERROR_REPR[e]);
}


