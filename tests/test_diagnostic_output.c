// test_diagnostic_output.c
// tests the output of the diagnostic functions in the library
// incapable of ascertaining success; always returns -1
#include <stdio.h>

#include <liblimeade/liblimeade.h>

int main(int argc, char **argv)
{
  limeade_enable_debugging();

  LIMEADE_CONTEXT c1, c2;

  // 1. host context
  c1 = limeade_host_init();
  limeade_diag_repr_context(c1);
  limeade_host_free(c1);
  limeade_diag_repr_context(c1);

  // 2. client context
  c2 = limeade_client_init(22, "roan@localhost");
  limeade_diag_repr_context(c2);
  limeade_client_free(c2);
  limeade_diag_repr_context(c2);

  //limeade_inserror(c2, LIMEADE_ERROR_INVALID_CONTEXT);

  limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
  limeade_inserror(LIMEADE_ERROR_GARBAGE);
  limeade_diag_repr_errors();

  printf("\nReturning -1\n");

  return -1;
}
