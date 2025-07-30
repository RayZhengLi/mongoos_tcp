# 主源文件
SOURCES1 = tcp_server.c mongoose/mongoose.c
SOURCES2 = tcp_client.c mongoose/mongoose.c

# 编译选项
CFLAGS = -W -Wall -Wextra -Werror -Wundef -Wshadow -g3 -O0 -I. -Imongoose
CFLAGS += -Wno-cast-function-type -DMG_ENABLE_OPENSSL=1 -DMG_TLS=MG_TLS_OPENSSL
LDFLAGS = -lpthread -lssl -lcrypto

# 链接库
LDFLAGS = -lpthread -lssl -lcrypto

# 构建目标
build: tcp_server tcp_client

tcp_server: $(SOURCES1)
	$(CC) $(SOURCES1) $(CFLAGS) $(MFLAGS) $(LDFLAGS) -o tcp_server

tcp_client: $(SOURCES2)
	$(CC) $(SOURCES2) $(CFLAGS) $(MFLAGS) $(LDFLAGS) -o tcp_client

# 运行（可选）
run:
	./tcp_server
	./tcp_client

# 清理
clean:
	rm -f tcp_server tcp_client
