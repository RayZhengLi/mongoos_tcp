# 可切换主程序（默认 tcp_client, 仅用于 all: 目标）
PROG ?= tcp_client

# 通用设置
DELETE = rm -rf
OUT = -o
CFLAGS = -W -Wall -Wextra -g -I.
LIBS = -lssl -lcrypto -ldl -lpthread
CFLAGS_MONGOOSE = -DMG_ENABLE_PACKED_FS=0 -DMG_TLS=MG_TLS_OPENSSL -lssl -lcrypto

# 源码文件
SRCS_SERVER = tcp_server.c mongoose.c
SRCS_CLIENT = tcp_client.c mongoose.c
SRCS_HTTPS_CLIENT = https_client.c mongoose.c

# 目标名
TARGETS = tcp_server tcp_client https_client

.PHONY: all clean run_server run_client

all: $(PROG)        # 默认只编译 PROG（可用 make PROG=tcp_server）

# 编译目标
tcp_server: $(SRCS_SERVER)
	$(CC) $(SRCS_SERVER) $(CFLAGS) $(CFLAGS_MONGOOSE) $(LIBS) $(OUT) tcp_server

tcp_client: $(SRCS_CLIENT)
	$(CC) $(SRCS_CLIENT) $(CFLAGS) $(CFLAGS_MONGOOSE) $(LIBS) $(OUT) tcp_client

https_client: $(SRCS_HTTPS_CLIENT)
	$(CC) $(SRCS_HTTPS_CLIENT) $(CFLAGS) $(CFLAGS_MONGOOSE) $(LIBS) $(OUT) https_client

# 一次性全编译
build_all: tcp_server tcp_client https_client

# 运行
run_server: tcp_server
	./tcp_server

run_client: tcp_client
	./tcp_client

run_https_client: https_client
	./https_client
# 清理
clean:
	$(DELETE) $(TARGETS) *.o *.obj *.exe *.dSYM
