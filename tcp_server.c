#include "mongoose.h"

static const char *s_lsn = "tls://0.0.0.0:9000";

static void cb(struct mg_connection *c, int ev, void *ev_data) {
  (void)ev_data;
  if (ev == MG_EV_ACCEPT) {
    MG_INFO(("Accepted new connection"));
    if (mg_url_is_ssl(s_lsn)) {
      struct mg_tls_opts opts = {0};
      opts.cert =
          mg_unpacked("/cert/ss_server.pem");
      opts.key =
          mg_unpacked("/cert/ss_server.pem");
      mg_tls_init(c, &opts);
    }
  } else if (ev == MG_EV_READ) {
    struct mg_iobuf *r = &c->recv;
    MG_INFO(("Received %.*s", (int)r->len, r->buf));
    mg_send(c, r->buf, r->len);  // Echo
    r->len = 0;
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("Connection closed"));
  }
}

int main(void) {
  struct mg_mgr mgr;
  mg_log_set(MG_LL_INFO);
  mg_mgr_init(&mgr);

  if (mg_listen(&mgr, s_lsn, cb, NULL) == NULL) {
    MG_ERROR(("Cannot bind to s_lsn %s", s_lsn));
    return 1;
  }

  MG_INFO(("Server started on %s", s_lsn));
  for (;;) mg_mgr_poll(&mgr, 100);

  mg_mgr_free(&mgr);
  return 0;
}
