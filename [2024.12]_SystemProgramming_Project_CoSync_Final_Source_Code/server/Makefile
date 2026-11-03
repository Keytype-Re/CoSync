# 컴파일러와 플래그
CC = gcc
CFLAGS = -Wall -pthread -g

# 프로젝트 파일
TARGET = server_app
SRC = main.c common.c broadcast.c chat_handler.c version_control.c chat_handler.c login_control.c file_sync.c
OBJ = $(SRC:.c=.o)

# 포함 디렉토리
INCLUDES = -I./includes

# 기본 타겟
all: $(TARGET)

# 실행 파일 빌드
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# 개별 소스 파일 빌드
main.o: main.c includes/common.h includes/login_control.h includes/chat_handler.h includes/file_sync.h includes/version_control.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

common.o: common.c includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

chat_handler.o: chat_handler.c includes/chat_handler.h includes/broadcast.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

broadcast.o: broadcast.c includes/broadcast.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

file_sync.o: file_sync.c includes/file_sync.h includes/common.h includes/broadcast.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

login_control.o: login_control.c includes/login_control.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

version_control.o: version_control.c includes/version_control.h includes/broadcast.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 클린업
clean:
	rm -f $(OBJ) $(TARGET)

# 디버깅용
print-%:
	@echo $* = $($*)
