/**
 * @author 2023012083 김무성
 */

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include "includes/receive_handler.h"
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
    else puts(path ? "Loaded shared file." : "Cleared shared file.");
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
    } else if (!strcmp(line, "/log")) packet.flag = COMMAND;
    else packet.flag = CHAT;
    strcpy(packet.message, line);
    send_packet(&packet);
}

static void submit_line(char *line)
{
    size_t length = strlen(line);
    if (length && line[length - 1] == '\r') line[--length] = 0;
    if (length >= BUFFER_SIZE) {
        fprintf(stderr, "Input exceeds the packet limit.\n");
        return;
    }
    if (*line == '/') {
        while (length && (line[length - 1] == ' ' || line[length - 1] == '\t'))
            line[--length] = 0;
    }
    if (length) handle_line(line);
}

static void edited_line(char *line)
{
    if (line) submit_line(line);
    else stop_client();
    free(line);
}

/* poll() keeps a partial network frame from blocking the next keystroke. */
int run_chat_ui(void)
{
    int interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    char prompt[sizeof(username) + 8];
    snprintf(prompt, sizeof(prompt), "[%s] : ", username);
    if (interactive) rl_callback_handler_install(prompt, edited_line);

    struct pollfd events[] = {{STDIN_FILENO, POLLIN, 0}, {client_socket, POLLIN, 0},
                              {stop_pipe[0], POLLIN, 0}};
    Packet incoming;
    size_t received = 0, used = 0;
    char line[BUFFER_SIZE + 1];
    int oversized = 0;
    int result = 0;
    while (keep_running) {
        int ready = poll(events, 3, -1);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { perror("poll"); result = -1; break; }
        if (events[2].revents) break;
        if (events[1].revents) {
            ssize_t n = recv(client_socket, (char *)&incoming + received,
                             sizeof(incoming) - received, MSG_DONTWAIT);
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            if (n <= 0) {
                fprintf(stderr, "Disconnected from server.\n");
                result = -1;
                break;
            }
            received += n;
            if (received == sizeof(incoming)) {
                int redraw = interactive && incoming.flag != FILE_UPDATE;
                if (redraw) rl_clear_visible_line();
                result = display_server_packet(&incoming);
                if (result < 0) break;
                if (redraw) { rl_on_new_line(); rl_redisplay(); }
                fflush(stdout);
                received = 0;
            }
        }
        if (events[0].revents) {
            if (interactive) {
                rl_callback_read_char();
                continue;
            }
            char c;
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) { perror("stdin"); result = -1; break; }
            if (!n || c == '\n') {
                line[used] = 0;
                if (oversized) fprintf(stderr, "Input exceeds the packet limit.\n");
                else submit_line(line);
                used = 0;
                oversized = 0;
                if (!n) break;
            } else if (used < BUFFER_SIZE) line[used++] = c;
            else oversized = 1;
        }
    }
    if (interactive) rl_callback_handler_remove();
    return result;
}

int terminal_columns(void)
{
    struct winsize size;
    return ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col ? size.ws_col : 80;
}

void print_centered(const char *text, int width)
{
    int padding = (width - (int)strlen(text)) / 2;
    printf("%*s%s\n", padding > 0 ? padding : 0, "", text);
}

void command_help(void)
{
    puts("[Commands]");
    puts("/new          Clear shared file");
    puts("/load PATH    Load a text file");
    puts("/commit       Save a version");
    puts("/log          List versions");
    puts("/rebase N     Restore version N");
    puts("/quit         Disconnect");
    puts("/help         Show commands");
}
