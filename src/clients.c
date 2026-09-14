// clients.c
// helper functions for managing the node-based ctx->clients
// 
// AGPL

#include<liblimeade/liblimeade.h>

// rule for everyone that touches this file: recursion is not allowed

void limeade_client_free(struct limeade_eth_client *head)
{
  register struct limeade_eth_client *a, *b;
  a = b = head;
  for(; b; free(a), a = b)
    b = a->next;
  head = NULL;
}

LIMEADE_SESSION limeade_gen_session(void)
{
  srand(time(NULL));
  LIMEADE_SESSION ret;
  ret  = (unsigned int)~rand();
  ret ^= (unsigned int)~rand() << 32;

  // is this edge-case even worth checking for?
  return ret ? ret != 0 : limeade_gen_session();
}

int limeade_client_preexisting(const struct limeade_eth_client *head,
                               struct sockaddr_in cliaddr,
                               LIMEADE_SESSION session)
{
  /* Ascertains if a client was already connected via comparing the IP address
   * & the session */
  if(!head)
    return -1;

  struct limeade_eth_client *prev;
  register struct limeade_eth_client *cur = head;

  do
  {
    prev = cur;

    if(cur->cliaddr.sin_addr.s_addr == cliaddr.sin_addr.s_addr
    && cur->session == session)
      return 1;

    cur = cur->next;
  } while(cur);

  return 0;

}

int limeade_client_check_session(const struct limeade_eth_client *head,
                                 LIMEADE_SESSION session)
{
  /* Ascertais if a specific session is being used */
  register struct limeade_eth_client *cur = head;

  do
  {
    if(cur->session == session)
      return 1;
    cur = cur->next;
  } while(cur);
  return 0;
}

int limeade_client_register(const struct limeade_eth_client *head,
                            struct sockaddr_in cliaddr, socklen_t len,
                            LIMEADE_SESSION session)
{
  /* Adds a client to the end of the client list */
  struct limeade_eth_client *cur, *prev;
  cur = head;

  do
  {
    prev = cur;
    cur  = cur->next;
  } while(cur);

  prev->next = malloc(sizeof(*cur));
  if(!prev->next)
    return -1;
  cur              = prev->next;
  cur->prev        = prev;
  memcpy(&cur->cliaddr, &cliaddr, sizeof(cliaddr));
  cur->cliaddr_len = len;
  cur->session     = session;
  cur->next        = NULL;

  return 0;
}

int limeade_client_check_register(const struct limeade_eth_client *head,
                                  struct sockaddr_in cliaddr,
                                  socklen_t len, LIMEADE_SESSION session)
{
  /* Manages client registration for the host.
   * If a client is not found within the database, a new client is created
   * within the database.
   *
   * IMPORTANT NOTE: while this function attempts to prevent spoofing attacks,
   * the UDP server mode has absolutely no guarantee of security; that's why
   * there's also an SSH subsystem. In short, we try our best.
   * 
   * RETURN VALUE:
   * > 0: the index of the newly created client node 
   * 0:   the data was attributed to a preexisting node
   * < 0:  an error occured (probably allocation failure)
   */

  if(!head)
    return -1;

  struct limeade_eth_client *prev;
  register struct limeade_eth_client *cur = head;

  do
  {
    prev = cur;

    if(cur->cliaddr.sin_addr.s_addr == cliaddr.sin_addr.s_addr)

      if(cur->session != session)
        return -1;

    else if(cur->session == session)

      if(cur->cliaddr.sin_addr.s_addr != cliaddr.sin_addr.s_addr)
        return -1;
    
    else // ip != ip && session != session
      goto invalid;

    goto update;

invalid:

    cur = cur->next;
    
  } while(cur);

  prev->next = malloc(sizeof(*prev));
  cur = prev->next;
  if(!cur)
    return -1;

  cur->idx = prev->idx + 1;
  cur->pkts_to = 0;
  cur->pkts_fr = 1;
  cur->session = session;
  memcpy(cur->cliaddr, cliaddr, sizeof(cliaddr));
  cur->cliaddr_len = sizeof(cliaddr);
  cur->prev = prev;
  cur->next = NULL;

  return 0;

update:

  cur->pkts_fr++;
  return 0;
}

