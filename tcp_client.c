#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#include "mongoose.h"

#define MAX_MSG_LEN 256
#define QUEUE_SIZE 16
static const char *s_conn = "tls://127.0.0.1:9000";

typedef struct {
  char msg[MAX_MSG_LEN];
  int len;
} msg_t;

typedef struct {
  msg_t queue[QUEUE_SIZE];
  int head;
  int tail;
  pthread_mutex_t lock;
} msg_buf_t;

// 简单环形缓冲区
msg_buf_t recv_buf = {.head = 0, .tail = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
msg_buf_t send_buf = {.head = 0, .tail = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

// 信号量
sem_t recv_sem;

static int want_close = 0;

static struct {
  struct mg_connection *c;
} c_res;

// 生产者线程：从stdin读入消息到队列
void *input(void *arg) {
  (void)arg;
  while (!want_close) {
    char buf[MAX_MSG_LEN];
    if (fgets(buf, sizeof(buf), stdin) == NULL) break;
    buf[strcspn(buf, "\n")] = 0;

    pthread_mutex_lock(&send_buf.lock);
    int next = (send_buf.head + 1) % QUEUE_SIZE;
    if (next != send_buf.tail) {  // 环形队列未满
      strncpy(send_buf.queue[send_buf.head].msg, buf, MAX_MSG_LEN - 1);
      send_buf.queue[send_buf.head].msg[MAX_MSG_LEN - 1] = 0;
      send_buf.head = next;
    }
    pthread_mutex_unlock(&send_buf.lock);

    if (strcmp(buf, "quit") == 0) {
      want_close = 1;
      break;
    }
  }
  return NULL;
}

// 生产者线程，用于发送echo
void *process(void *arg) {
  (void)arg;
  while (!want_close) {
    sem_wait(&recv_sem);
    msg_t msg;
    pthread_mutex_lock(&recv_buf.lock);
    if (recv_buf.tail != recv_buf.head) {
      memcpy(msg.msg, recv_buf.queue[recv_buf.tail].msg,
             recv_buf.queue[recv_buf.tail].len);
      msg.len = recv_buf.queue[recv_buf.tail].len;
      recv_buf.tail = (recv_buf.tail + 1) % QUEUE_SIZE;
      pthread_mutex_unlock(&recv_buf.lock);
      msg.msg[msg.len] = '\0';
      printf("[echo] %s\n", msg.msg);
    } else {
      pthread_mutex_unlock(&recv_buf.lock);
    }
  }
  return NULL;
}

// 客户端事件回调
static void cfn(struct mg_connection *c, int ev, void *ev_data) {
  (void)ev_data;
  if (ev == MG_EV_CONNECT) {
    MG_INFO(("CLIENT connected"));
    if (mg_url_is_ssl(s_conn)) {
      struct mg_tls_opts opts = {0};
      opts.ca = mg_unpacked("/cert/ss_ca.pem");
      mg_tls_init(c, &opts);
    }
    c_res.c = c;
  } else if (ev == MG_EV_READ) {
    struct mg_iobuf *r = &c->recv;
    MG_INFO(("CLIENT got: %.*s", (int)r->len, r->buf));

    pthread_mutex_lock(&recv_buf.lock);
    int next = (recv_buf.head + 1) % QUEUE_SIZE;
    if (next != recv_buf.tail) {  // 环形队列未满
      memcpy(recv_buf.queue[recv_buf.head].msg, r->buf, r->len);
      recv_buf.queue[recv_buf.head].msg[MAX_MSG_LEN - 1] = 0;
      recv_buf.queue[recv_buf.head].len = r->len;
      recv_buf.head = next;
    }
    pthread_mutex_unlock(&recv_buf.lock);

    sem_post(&recv_sem);
    r->len = 0;
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("CLIENT disconnected"));
    c_res.c = NULL;
  } else if (ev == MG_EV_POLL && c == c_res.c) {
    // 消费者：只在连接成功后发送队列内容
    pthread_mutex_lock(&send_buf.lock);
    while (send_buf.tail != send_buf.head && c_res.c != NULL) {
      mg_send(c, send_buf.queue[send_buf.tail].msg,
              strlen(send_buf.queue[send_buf.tail].msg));
      if (strcmp(send_buf.queue[send_buf.tail].msg, "quit") == 0) {
        want_close = 1;
        c->is_draining = 1;  // 发送完再优雅断开
      }
      send_buf.tail = (send_buf.tail + 1) % QUEUE_SIZE;
    }
    pthread_mutex_unlock(&send_buf.lock);
  }
}

// Timer function
static void timer_fn(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *)arg;
  if (c_res.c == NULL) {
    c_res.c = mg_connect(mgr, s_conn, cfn, &c_res);
    MG_INFO(("CLIENT %s", c_res.c ? "connecting" : "failed"));
  }
}

// Periodic send function
// static void periodic_send(void *arg) {
//   (void)arg;
//   char buf[MAX_MSG_LEN];
//   static int index = 0;
//   index++;
//   sprintf(buf, "This is the %ld periodic message!\n", mg_millis());
//   pthread_mutex_lock(&send_buf.lock);
//   int next = (send_buf.head + 1) % QUEUE_SIZE;
//   if (next != send_buf.tail) {  // 环形队列未满
//     strncpy(send_buf.queue[send_buf.head].msg, buf, MAX_MSG_LEN - 1);
//     send_buf.queue[send_buf.head].msg[MAX_MSG_LEN - 1] = 0;
//     send_buf.head = next;
//   }
//   pthread_mutex_unlock(&send_buf.lock);
// }

int main(void) {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);

  c_res.c = mg_connect(&mgr, s_conn, cfn, &c_res);
  if (!c_res.c) {
    fprintf(stderr, "Failed to connect to server\n");
    return 1;
  }

  sem_init(&recv_sem, 0, 0);
  pthread_t input_tid;
  pthread_create(&input_tid, NULL, input, NULL);
  pthread_t proc_tid;
  pthread_create(&proc_tid, NULL, process, NULL);

  mg_timer_add(&mgr, 1000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &mgr);
  // mg_timer_add(&mgr, 1000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, periodic_send,
  //              NULL);

  while (!want_close || c_res.c != NULL) mg_mgr_poll(&mgr, 100);

  sem_post(&recv_sem);
  pthread_join(input_tid, NULL);
  pthread_join(proc_tid, NULL);
  sem_destroy(&recv_sem);
  mg_mgr_free(&mgr);
  printf("Bye.\n");
  return 0;
}
