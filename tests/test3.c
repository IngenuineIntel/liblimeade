// test 3
// the test that is the third

#include<pthread.h>
#include<stdio.h>

#include<liblimeade/liblimeade.h>

int main()
{
  printf("sizeof(struct limeade_context)    = %lu\n", sizeof(struct limeade_context));
  printf("sizeof(struct limeade_recv_data)  = %lu\n", sizeof(struct limeade_recv_data));
  printf("sizeof(struct limeade_indiv_recv) = %lu\n", sizeof(struct limeade_indiv_recv));
  printf("sizeof(pthread_mutex_t)           = %lu\n", sizeof(pthread_mutex_t));

  return 0;
}
