/**
 * @author 2023012083 김무성
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "includes/send_handler.h"
#include "includes/file_monitor.h"

static void change_file(const char *path)
{
    char data[BUFFER_SIZE] = "";
    pthread_mutex_lock(&file_mutex);
    int result = path ? read_text_file(path, data, 1) : 0;
    if (result == 0) result = store_text_file(SHARED_FILE, data, 0);
    pthread_mutex_unlock(&file_mutex);
    if (result < 0) perror(path ? path : SHARED_FILE);
}

static void handle_line(const char *line)
{
    Packet packet = {0};
    if (!strcmp(line, "/quit")) { stop_client(); return; }
    if (!strcmp(line, "/help")) { command_help(); return; }
    if (!strcmp(line, "/new")) { change_file(NULL); return; }
    if (!strcmp(line, "/commit")) { send_current_file(1); return; }
    if (!strcmp(line, "/load") || !strncmp(line, "/load ", 6)) {
        const char *path = line + 5;
        while (*path == ' ') path++;
        if (*path) change_file(path);
        else fprintf(stderr, "Usage: /load PATH\n");
        return;
    }
    if (!strcmp(line, "/rebase") || !strncmp(line, "/rebase ", 8)) {
        int version;
        if (line[7] != ' ' || parse_version(line + 8, &version) < 0) {
            fprintf(stderr, "Usage: /rebase VERSION\n");
            return;
        }
        packet.flag = COMMAND;
    } else packet.flag = !strcmp(line, "/log") ? COMMAND : CHAT;
    strcpy(packet.message, line);
    send_packet(&packet);
}

void *send_terminal_packet(void *arg)
{
    (void)arg;
    char line[BUFFER_SIZE];
    size_t used = 0;
    int too_long = 0;
    printf("[%s] : ", username);
    fflush(stdout);
    while (wait_for_input(STDIN_FILENO)) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            if (n == 0 && used && !too_long) {
                line[used] = 0;
                handle_line(line);
            }
            break;
        }
        if (c != '\n') {
            if (used < sizeof(line) - 1) line[used++] = c;
            else too_long = 1;
            continue;
        }
        line[used] = 0;
        if (too_long) fprintf(stderr, "Input exceeds the packet limit.\n");
        else handle_line(line);
        used = 0;
        too_long = 0;
        if (!keep_running) break;
        printf("[%s] : ", username);
        fflush(stdout);
    }
    stop_client();
    return NULL;
}

void print_centered(const char *text, int width) {
    int len = strlen(text);
    int padding = (width - len) / 2;
    if (padding < 0) padding = 0; // Prevent negative padding
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }
    printf("%s\n", text);
}

void command_help() {
	struct winsize w = {.ws_col = 80};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col; // Get terminal width

    // 메뉴 상단
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("COMMAND HELP MENU", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");

    // 테이블 헤더
    printf("| %-19s | %-76s |\n", "Command", "Description");
    printf("%.*s\n", 102, "--------------------------------------------------------------------------------------------------------------------------------------------\n");

    // 명령어 목록
    printf("| %-19s | %-76s |\n", "/new", "Create a new shared file session");
    printf("| %-19s | %-76s |\n", "/load (filepath)", "Load an existing file to shared file session");
    printf("| %-19s | %-76s |\n", "/commit", "Commit shared file to the server");
    printf("| %-19s | %-76s |\n", "/rebase (version)", "Rebase to the file");
    printf("| %-19s | %-76s |\n", "/quit", "Exit the program");
    printf("| %-19s | %-76s |\n", "/help", "Show this help menu");

    // 메뉴 중간
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("NOTICE", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");

    // 공지 사항
    printf("| %-98s |\n", "1. Use commands with correct arguments.");
    printf("| %-98s |\n", "2. You should save the shared file before using any instructions.");

    printf("| %-19s | %-76s |\n", "/log", "Show saved versions");

    // 메뉴 하단
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    puts("");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////