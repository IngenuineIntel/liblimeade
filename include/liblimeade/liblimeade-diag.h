// liblimeade-diag.c
// diagnostic functions for liblimeade
// AGPL

extern enum limeade_error;
extern static const char LIMEADE_ERROR_REPR;

#include<stdio.h>

void limeade_diag_repr_error(enum limeade_error e);

