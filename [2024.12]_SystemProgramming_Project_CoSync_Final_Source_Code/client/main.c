/**
 * @author 2023016057 조신근
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "includes/common.h"
#include "includes/file_monitor.h"
#include "includes/receive_handler.h"
#include "includes/send_handler.h"

void print_init_logo(void);
void get_terminal_size(int *cols);

static int connect_server(void)
{
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(PORT)};
    const char *address = getenv("COSYNC_SERVER_ADDR");
    if (!address) address = "0.0.0.0";
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 server address.\n");
        return -1;
    }
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) return -1;
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(client_socket);
        return -1;
    }
    return 0;
}

static int read_username(FILE *file)
{
    if (!fgets(username, sizeof(username), file)) return -1;
    size_t len = strcspn(username, "\n");
    if (username[len] != '\n') {
        int c = fgetc(file);
        if (c != EOF && c != '\n') {
            while (c != EOF && c != '\n') c = fgetc(file);
            return -1;
        }
    }
    username[len] = 0;
    return valid_username(username, sizeof(username)) ? 0 : -1;
}

static int login_client(void)
{
    FILE *saved = fopen(USER_FILE, "r");
    int signup = saved == NULL;
    if (signup && errno != ENOENT) { perror(USER_FILE); return -1; }
    if (saved) {
        int result = read_username(saved);
        fclose(saved);
        if (result < 0) { fprintf(stderr, "Invalid cached username.\n"); return -1; }
    }
    print_init_logo();
    for (;;) {
        if (signup) {
            printf(">> Enter your username: ");
            fflush(stdout);
            if (read_username(stdin) < 0) {
                fprintf(stderr, "Enter a name of 1 to 49 bytes without commas.\n");
                if (feof(stdin) || ferror(stdin)) return -1;
                continue;
            }
        }
        Packet request = {.flag = signup ? LOGIN_NEW : CHAT};
        strcpy(request.username, username);
        if (send_all(client_socket, &request, sizeof(request), 0) < 0) return -1;
        unsigned char first;
        ssize_t n;
        do { n = recv(client_socket, &first, 1, MSG_PEEK); } while (n < 0 && errno == EINTR);
        if (n != 1) return -1;
        if (first == 'F') {
            fprintf(stderr, "Server is full. Try again after another user disconnects.\n");
            return -1;
        }
        if (!signup) {
            if (first == 'N') {
                fprintf(stderr, "Server does not recognize the cached username.\n");
                return -1;
            }
            return 0;
        }
        const char *reply = first == 'D' ? "DUPLICATE" : "REGISTERED";
        char buffer[sizeof("REGISTERED")];
        size_t length = strlen(reply);
        if (recv_all(client_socket, buffer, length, 0) <= 0 || memcmp(buffer, reply, length))
            return -1;
        if (first == 'D') {
            puts("Username already exists. Try another.");
            continue;
        }
        char record[sizeof(username) + 1];
        snprintf(record, sizeof(record), "%s\n", username);
        if (store_text_file(USER_FILE, record, 0) < 0) { perror(USER_FILE); return -1; }
        return send_all(client_socket, "INIT_REQUEST", 12, 0) < 0 ? -1 : 0;
    }
}

static int receive_initial_data(void)
{
    Packet history, shared;
    if (recv_all(client_socket, &history, sizeof(history), 0) <= 0 ||
        history.flag != CHAT || !valid_payload(&history) ||
        recv_all(client_socket, &shared, sizeof(shared), 0) <= 0 ||
        shared.flag != FILE_UPDATE || !valid_payload(&shared)) return -1;
    if (apply_to_file(shared.file_data) < 0) return -1;
    puts(">> Login Complete\n\n[ChatLog]");
    for (char *line = strtok(history.message, "\n"); line; line = strtok(NULL, "\n")) {
        char *comma = strchr(line, ',');
        if (!comma) continue;
        *comma++ = 0;
        if (!strcmp(username, line)) printf("[%s] : %s\n", line, comma);
        else print_right_aligned(line, comma);
    }
    return 0;
}

static int run_session(void)
{
    int monitor = init_inotify();
    if (monitor < 0) { perror("inotify"); return -1; }
    if (pipe(stop_pipe) < 0) { close(monitor); return -1; }
    pthread_t watcher, terminal;
    int watching = pthread_create(&watcher, NULL, watch_file, &monitor) == 0;
    int reading = watching && pthread_create(&terminal, NULL, send_terminal_packet, NULL) == 0;
    int result = reading ? receive_server_packets() : -1;
    stop_client();
    if (reading) pthread_join(terminal, NULL);
    if (watching) pthread_join(watcher, NULL);
    close(stop_pipe[0]);
    close(monitor);
    return result;
}

int main(void)
{
    /* Login reads lines; the terminal thread subsequently polls the same fd. */
    setvbuf(stdin, NULL, _IONBF, 0);
    if (connect_server() < 0) { perror("connect"); return EXIT_FAILURE; }
    int result = -1;
    if (mkdir("./watch", 0700) < 0 && errno != EEXIST) perror("./watch");
    else if (login_client() == 0) {
        if (receive_initial_data() == 0) result = run_session();
        else fprintf(stderr, "Could not load initial chat history and shared file.\n");
    }
    close(client_socket);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void print_init_logo()
{
    struct winsize w = {.ws_col = 80};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col; // 터미널 너비 가져오기

    // 상단 구분선
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    
    // 로고 출력 (백슬래시 이스케이프 수정)
    print_centered(" ____               ____                                   ", term_width);
    print_centered("/\\  _`\\            /\\  _`\\                                 ", term_width);
    print_centered("\\ \\ \\\\\\/_\\    ___   \\ \\,\\L\\_\\    __  __      ___      ___   ", term_width);
    print_centered(" \\ \\ \\/_/_  / __`\\  \\/_\\__ \\   /\\ \\\\ /\\ \\   /' _ `\\   /'___\\ ", term_width);
    print_centered("  \\ \\ \\L\\ \\\\ \\ \\L\\ \\   /\\ \\L\\ \\ \\ \\ \\_\\ \\  /\\ \\/\\ \\ /\\ \\__/ ", term_width);
    print_centered("   \\ \\____/\\ \\____/   \\ `\\____\\ \\/`____ \\ \\ \\_\\ \\_\\\\ \\____\\", term_width);
    print_centered("    \\/___/  \\/___/     \\/_____/  `/___/> \\ \\/_/\\/_/ \\/____/", term_width);
    print_centered("                                    /\\___/                 ", term_width);
    print_centered("                                    \\/__/                  ", term_width);
    print_centered("", term_width);
    
    // 하단 구분선
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    
    // 환영 메시지 및 정보
    print_centered("                 Welcome to the CoSync!                     ", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("              Version      : 1.0.0                          ", term_width);
    print_centered("              Author       : Third Team                     ", term_width);
    print_centered("              Description  : File Sync with chat            ", term_width);
    print_centered("              Date Created : 2024-12-13                     ", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    puts("");
    printf(">> Insert \"help\" if you want help\n");
    printf(">> Hello! %s\n", username);
}

void get_terminal_size(int *cols)
{
    struct winsize w = {.ws_col = 80};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *cols = w.ws_col;
}

// Function to print right-aligned message
void print_right_aligned(const char *name, const char *message)
{
    int cols;
    get_terminal_size(&cols);

    char formatted_message[BUFFER_SIZE + 50 + 4];
    snprintf(formatted_message, sizeof(formatted_message), "%s : [%s]", message, name);

    int padding = cols - (int)strlen(formatted_message);


    if (padding > 0)
    {
        for (int i = 0; i < padding; i++)
        {
            printf(" ");
        }
    }
    printf("%s\n", formatted_message);
}
