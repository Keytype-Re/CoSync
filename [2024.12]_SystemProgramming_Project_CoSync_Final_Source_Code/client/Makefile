# 컴파일러와 플래그
CC = gcc
CFLAGS = -Wall -pthread -g

# 프로젝트 파일
TARGET = client_app
SRC = main.c common.c file_monitor.c receive_handler.c send_handler.c
OBJ = $(SRC:.c=.o)

# 포함 디렉토리
INCLUDES = -I./includes

# 기본 타겟
all: $(TARGET)

# 실행 파일 빌드
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# 개별 소스 파일 빌드
main.o: main.c includes/common.h includes/file_monitor.h includes/receive_handler.h includes/send_handler.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

common.o: common.c includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

file_monitor.o: file_monitor.c includes/file_monitor.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

receive_handler.o: receive_handler.c includes/receive_handler.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

send_handler.o: send_handler.c includes/send_handler.h includes/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 클린업
clean:
	rm -f $(OBJ) $(TARGET)

# 디버깅용
print-%:
	@echo $* = $($*)
