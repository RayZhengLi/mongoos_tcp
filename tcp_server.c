// Copyright (c) 2022 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"

static const char *s_lsn = "tls://localhost:8765";   // Listening address
static const char *s_conn = "tls://localhost:8765";  // Connect to address

#ifdef USING_OPENSSL
struct mg_str ss_ca;
struct mg_str ss_server;
struct mg_str ss_client;
#else
static struct mg_str s_ca, s_srv_cert, s_srv_key;
static struct mg_str s_cli_cert, s_cli_key;
#endif

// client resources
static struct c_res_s {
  int i;
  struct mg_connection *c;
} c_res;

// CLIENT event handler
static void cfn(struct mg_connection *c, int ev, void *ev_data) {
  int *i = &((struct c_res_s *)c->fn_data)->i;
  if (ev == MG_EV_OPEN) {
    MG_INFO(("CLIENT has been initialized"));
  } else if (ev == MG_EV_CONNECT) {
    MG_INFO(("CLIENT connected"));
    if (mg_url_is_ssl(s_conn)) {
      struct mg_tls_opts opts = {
        #ifdef USING_OPENSSL
          .ca = ss_ca,
        #else
          .ca = s_ca,
          .name = mg_str("localhost")
        #endif
          // .cert = ss_client,
          // .key = ss_client
      };
      mg_tls_init(c, &opts);
    }
    *i = 1;  // do something
  } else if (ev == MG_EV_READ) {
    struct mg_iobuf *r = &c->recv;
    MG_INFO(("CLIENT got data: %.*s", r->len, r->buf));
    r->len = 0;  // Tell Mongoose we've consumed data
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("CLIENT disconnected"));
    // signal we are done
    ((struct c_res_s *)c->fn_data)->c = NULL;
  } else if (ev == MG_EV_ERROR) {
    MG_INFO(("CLIENT error: %s", (char *)ev_data));
  } else if (ev == MG_EV_POLL && *i != 0) {
    switch ((*i)++) {
      case 50:  // 50 x 100ms = 5s
        mg_send(c, "Hi, there", 9);
        MG_INFO(("CLIENT sent data"));
        break;
      case 100:  // another 5s
        // send any possible outstanding data and close the connection
        c->is_draining = 1;
        break;
    }
  }
}

// SERVER event handler
static void sfn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_OPEN && c->is_listening == 1) {
    MG_INFO(("SERVER is listening"));
  } else if (ev == MG_EV_ACCEPT) {
    MG_INFO(("SERVER accepted a connection"));
    if (mg_url_is_ssl(s_lsn)) {
      struct mg_tls_opts opts = {//.ca = ss_ca,
                                #ifdef USING_OPENSSL
                                 .cert = ss_server,
                                 .key  = ss_server
                                #else
                                 .cert = s_srv_cert,
                                 .key  = s_srv_key
                                #endif
                                };
      mg_tls_init(c, &opts);
    }
  } else if (ev == MG_EV_READ) {
    struct mg_iobuf *r = &c->recv;
    MG_INFO(("SERVER got data: %.*s", r->len, r->buf));
    mg_send(c, r->buf, r->len);  // echo it back
    r->len = 0;                  // Tell Mongoose we've consumed data
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("SERVER disconnected"));
  } else if (ev == MG_EV_ERROR) {
    MG_INFO(("SERVER error: %s", (char *)ev_data));
  }
}

// Timer function - recreate client connection if it is closed
static void timer_fn(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *)arg;
  if (c_res.c == NULL) {
    c_res.i = 0;
    c_res.c = mg_connect(mgr, s_conn, cfn, &c_res);
    MG_INFO(("CLIENT %s", c_res.c ? "connecting" : "failed"));
  }
}

int main(void) {
  struct mg_mgr mgr;  // Event manager
  struct mg_connection *c;

#ifdef USING_OPENSSL
  // // Using openSSL
  // ss_ca = mg_file_read(&mg_fs_posix, "certs/ss_ca.pem");
  // ss_server = mg_file_read(&mg_fs_posix, "certs/ss_server.pem");
  // ss_client = mg_file_read(&mg_fs_posix, "certs/ss_client.pem");
#else
  // Using built-in TSL
  s_ca       = mg_file_read(&mg_fs_posix, "certs/new/ca.crt");
  s_srv_cert = mg_file_read(&mg_fs_posix, "certs/new/server.crt");
  s_srv_key  = mg_file_read(&mg_fs_posix, "certs/new/server.key");
  s_cli_cert = mg_file_read(&mg_fs_posix, "certs/new/client.crt");   // 仅在双向TLS需要
  s_cli_key  = mg_file_read(&mg_fs_posix, "certs/new/client.key");   // 仅在双向TLS需要
#endif

  mg_log_set(MG_LL_INFO);  // Set log level
  mg_mgr_init(&mgr);       // Initialize event manager

  mg_timer_add(&mgr, 15000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &mgr);
  c = mg_listen(&mgr, s_lsn, sfn, NULL);  // Create server connection
  if (c == NULL) {
    MG_INFO(("SERVER cant' open a connection"));
    return 0;
  }
  while (true)
    mg_mgr_poll(&mgr, 100);  // Infinite event loop, blocks for upto 100ms
                             // unless there is network activity

#ifdef USING_OPENSSL
  mg_free(&ss_ca);
  mg_free(&ss_client);
  mg_free(&ss_server);
#else
  mg_free((void*)s_ca.buf);
  mg_free((void*)s_srv_cert.buf);
  mg_free((void*)s_srv_key.buf);
  mg_free((void*)s_cli_cert.buf);
  mg_free((void*)s_cli_key.buf);
#endif

  mg_mgr_free(&mgr);  // Free resources
  return 0;
}