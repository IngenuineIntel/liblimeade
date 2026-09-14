// diag.c

// AGPL

#include<liblimeade/liblimeade-diag.h>

void limeade_perror(const char *s, enum limeade_error e)
{
  if(!s || (char)*s == '\0' || e >= LIMEADE_MAXIMUM_ERROR)
    return;

  printf("%s: %s\n", s, LIMEADE_ERROR_REPRS[e]);
}

void limeade_diag_error(enum limeade_error x)
{
  printf("enum limeade_error x = ");
  switch(x)
  {
    case LIMEADE_SUCCESS:
      printf("LIMEADE_OK;\n");
    case LIMEADE_ERROR_GARBAGE:
      printf("LIMEADE_GARBAGE;\n");
    case LIMEADE_ERROR_LIBSSH:
      printf("LIMEADE_ERROR_LIBSSH;\n");
    case LIMEADE_ERROR_SSH_CHILD:
      printf("LIMEADE_ERROR_SSH_CHILD;\n");
    case LIMEADE_ERROR_MONOTONIC:
      printf("LIMEADE_ERROR_MONOTONIC;\n");
    case LIMEADE_ERROR_NETWORK:
      printf("LIMEADE_ERROR_NETWORK;\n");
    case LIMEADE_ERROR_CSM:
      printf("LIMEADE_ERROR_CSM;\n");
    case LIMEADE_ERROR_NO_DATA:
      printf("LIMEADE_ERROR_NO_DATA;\n");
    case LIMEADE_ERROR_BAD_MAGIC:
      printf("LIMEADE_ERROR_BAD_MAGIC;\n");
    case LIMEADE_ERROR_BAD_FORMAT:
      printf("LIMEADE_ERROR_BAD_FORMAT;\n");
    case LIMEADE_ERROR_BAD_DATA:
      printf("LIMEADE_ERROR_BAD_DATA;\n");
    case LIMEADE_ERROR_BAD_COMPRESSION:
      printf("LIMEADE_ERROR_BAD_COMPRESSION;\n");
    case LIMEADE_ERROR_REJECTED:
      printf("LIMEADE_ERROR_REJECTED;\n");
    case LIMEADE_ERROR_INVALID_CONTEXT:
      printf("LIMEADE_ERROR_INVALID_CONTEXT;\n");
    case LIMEADE_ERROR_MEMORY:
      printf("LIMEADE_ERROR_MEMORY;\n");
    case LIMEADE_ERROR_NOT_SUPPORTED:
      printf("LIMEADE_ERROR_NOT_SUPPORTED;\n");
    case LIMEADE_ERROR_OTHER:
      print("LIMEADE_ERROR_OTHER;\n");
    case LIMEADE_MAXIMUM_ERROR:
      print("LIMEADE_MAXIMUM_ERROR;\n");
    default:
      printf("%i; // unknown\n", x);
  }
}

void limeade_diag_csm_compression_entry(struct limeade_csm_compression_entry x)
{
  printf("struct limeade_csm_compression_entry x = {\n\
    .compr_lvl    = (uint32_t)%u,\n\
    .precompr_sz  = (uint32_t)%u,\n\
    .postcompr_sz = (uint32_t)%u,\n\
    .elapsed_ms   = (uint32_t)%u,\n\
};\n", x.compr_lvl, x.precompr_sz, x.postcompr_sz, x.elapsed_ms);
}

void limeade_diag_csm_latency_entry(struct limeade_csm_latency_entry x)
{
  printf("struct limeade_csm_latency_entry x = {\n\
    .send_sz = (uint32_t)%u,\n\
    .elapsed_ms = (uint32_t)%u,\n\
};\n", x.send_sz, x.elapsed_ms);
}

void limeade_diag_csm_data(struct limeade_csm_data x)
{
  printf("struct limeade_csm_data x = {\n\
    .hist_compr      = (struct limeade_csm_compression_entry*)%p,\n\
    .hist_latent     = (struct limeade_csm_latency_entry*)%p,\n\
    .hist_compr_sz   = (uint32_t)%u,\n\
    .hist_latent_sz  = (uint32_t)%u,\n\
    .hist_compr_idx  = (uint32_t)%u,\n\
    .hist_latent_idx = (uint32_t)%u,\n\
    .freq_s          = (float)%f,\n\
    .id              = (void*)%p,\n\
};\n", x.hist_compr, x.hist_latent, x.hist_compr_sz, x.latent_sz,
       x.hist_compr_idx, x.hist_latent_idx, x.freq_s, x.id);
}

void limeade_diag_indiv_recv(struct limeade_indiv_recv x)
{
  printf("struct limeade_indiv_recv x = {\n\
    .data  = (void*)%p,\n\
    .sz    = (uint16_t)%u,\n\
    .mtx   = (pthread_mutex_t*)%p,\n\
    .flags = (uint8_t)%x,\n\
};\n", x.data, x.sz, x.mtx, x.flags);
}

void limeade_diag_recv_data(struct limeade_recv_data x)
{
  printf("struct limeade_recv_data x = {\n\
    .tid       = (pthread_t*)%p,\n\
    .nr_pkts   = (uint16_t)%u,\n\
    .read_idx  = (uint16_t)%u,\n\
    .wr_idx    = (uint16_t)%u,\n\
    .pkts_lost = (uint16_t)%u,\n\
    .hz        = (uint16_t)%u,\n\
    .pkts      = (struct limeade_indiv_recv*)%p\n\
};\n", x.tid, x.nr_pkts, x.read_idx, x.wr_idx, x.pkts_lost, x.hz, x.pkts);
}

void limeade_diag_eth_host_indiv_client(struct limeade_eth_host_indiv_client x)
{
  printf("struct limeade_eth_host_indiv_client x = {\n\
    .session = (uint64_t)%p,\n\
    .addr    = (struct sockaddr_in)%p,\n\
};\n", x.session, x.addr);
}

void limeade_diag_context(struct limeade_context x)
{
  /* NOTE 
   * BIG NOTE
   * VERY IMPORTANT NOTE
   * this function doesn't obey mutexes */
  printf("struct limeade_context x = {\n\
    .mode = (uint8_t)%u,\n\
    .compr_mod = (uint8_t)%u,\n\
    .compr_lvl = (uint8_t)%u,\n\
    .sfd = (int)%i,\n\
    .rfd = (int)%i,\n", x.mode, x.compr_mode, x.compr_lvl, x.sfd, x.rfd);

  switch(x.mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      printf(".ssh_pid = (pid_t)%u,\n", x.ssh_pid);
    case LIMEADE_MODE_CLIENT_LIBSSH:
      printf(".ssh_data = (void*)%p,\n", x.ssh_data);
    case LIMEADE_MODE_CLIENT_ETH:
      printf(".saddr = (struct sockaddr_in*)%p,\n.saddr_len = (socklen_t)%i,\n", x.saddr, x.saddr_len);
    case LIMEADE_MODE_HOST_SSH:
      do {} while(0);
    case LIMEADE_MODE_HOST_ETH:
      printf(".saddr = (struct sockaddr_in*)%p,\n.saddr_len = (socklen_t)%i,\n", x.saddr, x.saddr_len);
      printf(".nr_clients = (uint32_t)%u,\n.clients = (struct limeade_eth_host_indiv_client*)%p,\n", x.nr_clients, x.clients);
    default:
      printf("// invalid mode, ignoring mode-specific union data\n");
  }

  printf(".recv = (struct limeade_recv_data*)%p,\n\
    .csm = (struct limeade_csm_data*)%p,\n\
    .destination = \"%s\",\n\
    .port = (int)%p,\n\
    .sessionid = (uint64_t)%p,\n\
    .ack_wait_time_ms = (uint32_t)%u,\n\
    .retry_interval = (uint32_t)%u,\n\
    .mtx_sfd = (pthread_mutex_t*)%p,\n\
    .mtx_rfd = (pthread_mutex_t*)%p,\n\
    .mtx_mode_union = (pthread_mutex_t*)%p,\n\
    .mtx_compr = (pthread_mutex_t*)%p,\n\
    .mtx_th_csm = (pthread_mutex_t*)%p,\n\
    .mtx_pub = (pthread_mutex_t*)%p,\n\
};\n", x.recv, x.csm, x.port, x.sessionid, x.ack_wait_time_ms, x.retry_interval,
       x.mtx_sfd, x.mtx_rfd, x.mtx_mode_union, x.mtx_compr, x.mtx_th_csm, x.mtx_pub);
}

static void limeade_print_eval_packet(enum limeade_packet x)
{
  switch(x.type)
  {
    case LIMEADE_PACKET_KNOCK:
      printf("LIMEADE_PACKET_KNOCK");
    case LIMEADE_PACKET_RECOGNIZE:
      printf("LIMEADE_PACKET_RECOGNIZE");
    case LIMEADE_PACKET_INTRODUCTION:
      printf("LIMEADE_PACKET_INTRODUCTION");
    case LIMEADE_PACKET_ACKNOWLEDGE:
      printf("LIMEADE_PACKET_ACKNOWLEDGE");
    case LIMEADE_PACKET_EVENTS:
      printf("LIMEADE_PACKET_EVENTS");
    case LIMEADE_PACKET_PROC_GENERIC:
      printf("LIMEADE_PACKET_PROC_GENERIC");
    case LIMEADE_PACKET_PROC_UPDATE:
      printf("LIMEADE_PACKET_PROC_UPDATE");
    case LIMEADE_PACKET_PERF:
      printf("LIMEADE_PACKET_PERF");
    case LIMEADE_PACKET_COMMANDEER:
      printf("LIMEADE_PACKET_COMMANDEER");
    case LIMEADE_PACKET_EXITED:
      printf("LIMEADE_PACKET_EXITED");
    case LIMEADE_PACKET_CLOSE:
      printf("LIMEADE_PACKET_CLOSE");
    case LIMEADE_PACKET_MAX:
      printf("LIMEADE_PACKET_MAX");
    default:
      printf("%u", x.type);
  }
}

void limeade_diag_packet(enum limeade_packet x)
{
  printf("enum limeade_packet x = ");
  limeade_print_eval_packet(x);
  printf(";\n");
}

void limeade_diag_packet_data(struct limeade_packet_data x)
{
  printf("struct limeade_packet_data x = {\n\
    .type = (uint8_t)");
  limeade_print_eval_packet(x.type);
  printf(",\n.compr = (uint8_t)%u,\n\
    .pkt_sz = (uint32_t)%u,\n\
    .pkts = (void*)%p,\n\
    .data = (void*)%p,\n\
};\n", x.compr, x.pkt_sz, x.pkt, x.data);
}

void limeade_diag_packet_flags(struct limeade_packet_flags x)
{
  printf("struct limeade_packet_flags x = {\n\
    .packet_size = (uint16_t)%u,\n\
    .type:4      = (uint8_t)", x.packet_size);
  limeade_print_eval_packet(x.type);
  printf(",\nx.compr_lvl:4 = (uint8_t)%u\n\
    .ts_s = (uint32_t)%u\n\
    .ts_ms = (uint16_t)%u\n\
    .reserved = (uint8_t[7])0,\n\
    .session = (uint64_t)%lu\n\
};", x.compr_lvl, x.ts_s, x.ts_ms, x.session);
}

// TODO

}

