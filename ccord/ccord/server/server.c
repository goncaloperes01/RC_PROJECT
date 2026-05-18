#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define PORT 9000
#define BUF_SIZE 1024
#define MAX_CLIENTS 50
#define MAX_USERNAME 50
#define MAX_CHANNEL 50

#define USERS_FILE "../data/users.txt"
#define MESSAGES_FILE "../data/messages.txt"
#define TEMP_FILE "../data/temp.txt"

#define SERVER_VERSION "Server v3.0 - select(), real-time channels, UDP file transfer"

typedef struct {
    int fd;
    int logged_in;
    char username[MAX_USERNAME];
    char current_channel[MAX_CHANNEL];
    char ip[INET_ADDRSTRLEN];
    int udp_port;
} Client;

Client clients[MAX_CLIENTS];

void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

void send_response(int client_fd, const char *msg) {
    if (client_fd >= 0 && msg != NULL) {
        send(client_fd, msg, strlen(msg), 0);
    }
}

void ensure_data_folder(void) {
    mkdir("../data", 0777);
}

int parse_user_line(char *line, char *user, char *pass, int *status) {
    int fields = sscanf(line, "%99s %99s %d", user, pass, status);

    if (fields == 3) return 1;

    if (fields == 2) {
        *status = 1; // formato antigo: username password
        return 1;
    }

    return 0;
}

int user_exists(const char *username) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) return 0;

    char line[300];
    char user[100], pass[100];
    int status;

    while (fgets(line, sizeof(line), file)) {
        if (parse_user_line(line, user, pass, &status)) {
            if (strcmp(user, username) == 0) {
                fclose(file);
                return 1;
            }
        }
    }

    fclose(file);
    return 0;
}

int register_user(const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE, "a");
    if (!file) return 0;

    // status 0 = pendente; só o admin pode aprovar
    fprintf(file, "%s %s %d\n", username, password, 0);
    fclose(file);
    return 1;
}

int check_login(const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) return 0;

    char line[300];
    char user[100], pass[100];
    int status;

    while (fgets(line, sizeof(line), file)) {
        if (parse_user_line(line, user, pass, &status)) {
            if (strcmp(user, username) == 0 && strcmp(pass, password) == 0) {
                fclose(file);
                return status == 1 ? 1 : 2;
            }
        }
    }

    fclose(file);
    return 0;
}

int is_admin_credentials(const char *username, const char *password) {
    return strcmp(username, "admin") == 0 && check_login(username, password) == 1;
}

int is_admin_client(int idx) {
    return clients[idx].logged_in && strcmp(clients[idx].username, "admin") == 0;
}


void list_all_users(int client_fd, int show_pending) {
    FILE *file = fopen(USERS_FILE, "r");

    if (!file) {
        send_response(client_fd, "Erro ao abrir users.txt\n");
        return;
    }

    char line[300];
    char user[100], pass[100];
    int status;

    char approved[BUF_SIZE] = "Utilizadores aprovados:\n";
    char pending[BUF_SIZE] = "Utilizadores por aprovar:\n";

    int approved_count = 0;
    int pending_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%99s %99s %d", user, pass, &status) == 3) {
            if (status == 1) {
                strcat(approved, "\n- ");
                strcat(approved, user);
            } 
            else if (status == 0 && show_pending) {
                strcat(pending, "\n- ");
                strcat(pending, user);
                pending_count++;
            }

            if (status == 1) {
                approved_count++;
            }
        }
    }

    fclose(file);

    char response[BUF_SIZE * 2];
    response[0] = '\0';

    if (approved_count == 0) {
        strcat(response, "Sem utilizadores aprovados\n");
    } else {
        strcat(response, approved);
        strcat(response, "\n");
    }

    if (show_pending) {
        strcat(response, "\n");

        if (pending_count == 0) {
            strcat(response, "Sem utilizadores por aprovar\n");
        } else {
            strcat(response, pending);
            strcat(response, "\n");
        }
    }

    send_response(client_fd, response);
}

void store_message(int client_fd, const char *sender, const char *receiver, const char *message) {
    if (!user_exists(receiver)) {
        send_response(client_fd, "USER_NOT_FOUND\n");
        return;
    }

    FILE *file = fopen(MESSAGES_FILE, "a");
    if (!file) {
        send_response(client_fd, "Erro ao abrir messages.txt\n");
        return;
    }

    fprintf(file, "%s|%s|%s\n", sender, receiver, message);
    fclose(file);

    send_response(client_fd, "MSG_STORED\n");
}

void check_inbox(int client_fd, const char *username) {
    FILE *file = fopen(MESSAGES_FILE, "r");

    if (!file) {
        send_response(client_fd, "Inbox vazia\n");
        return;
    }

    char line[1200];
    char response[BUF_SIZE] = "";
    int count = 0;

    while (fgets(line, sizeof(line), file)) {
        char sender[50], receiver[50], msg[900];

        if (sscanf(line, "%49[^|]|%49[^|]|%899[^\n]", sender, receiver, msg) == 3) {
            if (strcmp(receiver, username) == 0) {
                char entry[1000];
                snprintf(entry, sizeof(entry), "%s: %s\n", sender, msg);

                if (strlen(response) + strlen(entry) < BUF_SIZE - 1) {
                    strncat(response, entry, BUF_SIZE - strlen(response) - 1);
                }
                count++;
            }
        }
    }

    fclose(file);

    if (count == 0) strcpy(response, "Inbox vazia\n");
    send_response(client_fd, response);
}

void approve_user(int client_fd, const char *target_user) {
    FILE *file = fopen(USERS_FILE, "r");
    FILE *temp = fopen(TEMP_FILE, "w");

    if (!file || !temp) {
        if (file) fclose(file);
        if (temp) fclose(temp);
        send_response(client_fd, "Erro ao abrir ficheiros\n");
        return;
    }

    char line[300];
    char user[100], pass[100];
    int status;
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (parse_user_line(line, user, pass, &status)) {
            if (strcmp(user, target_user) == 0) {
                status = 1;
                found = 1;
            }
            fprintf(temp, "%s %s %d\n", user, pass, status);
        }
    }

    fclose(file);
    fclose(temp);

    remove(USERS_FILE);
    rename(TEMP_FILE, USERS_FILE);

    send_response(client_fd, found ? "USER_APPROVED\n" : "USER_NOT_FOUND\n");
}

void delete_user(int client_fd, const char *target_user) {
    if (strcmp(target_user, "admin") == 0) {
        send_response(client_fd, "CANNOT_DELETE_ADMIN\n");
        return;
    }

    FILE *file = fopen(USERS_FILE, "r");
    FILE *temp = fopen(TEMP_FILE, "w");

    if (!file || !temp) {
        if (file) fclose(file);
        if (temp) fclose(temp);
        send_response(client_fd, "Erro ao abrir ficheiros\n");
        return;
    }

    char line[300];
    char user[100], pass[100];
    int status;
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (parse_user_line(line, user, pass, &status)) {
            if (strcmp(user, target_user) == 0) {
                found = 1;
                continue;
            }
            fprintf(temp, "%s %s %d\n", user, pass, status);
        }
    }

    fclose(file);
    fclose(temp);

    remove(USERS_FILE);
    rename(TEMP_FILE, USERS_FILE);

    send_response(client_fd, found ? "USER_DELETED\n" : "USER_NOT_FOUND\n");
}

void init_clients(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].logged_in = 0;
        clients[i].username[0] = '\0';
        clients[i].current_channel[0] = '\0';
        clients[i].ip[0] = '\0';
        clients[i].udp_port = 0;
    }
}

int find_free_client_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) return i;
    }
    return -1;
}

int find_online_user(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].logged_in && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

void broadcast_to_channel(const char *channel, const char *message) {
    if (channel == NULL || channel[0] == '\0') return;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].logged_in && strcmp(clients[i].current_channel, channel) == 0) {
            send_response(clients[i].fd, message);
        }
    }
}

void remove_client(int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS || clients[idx].fd == -1) return;

    char left_msg[200];
    if (clients[idx].logged_in && clients[idx].current_channel[0] != '\0') {
        snprintf(left_msg, sizeof(left_msg), "[SERVER] %s saiu do canal %s\n", clients[idx].username, clients[idx].current_channel);
        broadcast_to_channel(clients[idx].current_channel, left_msg);
    }

    printf("Cliente desligado: fd=%d user=%s\n", clients[idx].fd, clients[idx].username);
    close(clients[idx].fd);

    clients[idx].fd = -1;
    clients[idx].logged_in = 0;
    clients[idx].username[0] = '\0';
    clients[idx].current_channel[0] = '\0';
    clients[idx].ip[0] = '\0';
    clients[idx].udp_port = 0;
}

void handle_join(int idx, const char *channel_arg) {
    if (!clients[idx].logged_in) {
        send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        return;
    }

    if (channel_arg == NULL || channel_arg[0] == '\0') {
        send_response(clients[idx].fd, "Uso: /join #canal\n");
        return;
    }

    char clean[MAX_CHANNEL];

    // Normalização do canal:
    // /join general   -> #general
    // /join #general  -> #general
    // /join ##general -> #general
    while (*channel_arg == '#') {
        channel_arg++;
    }

    if (channel_arg[0] == '\0') {
        send_response(clients[idx].fd, "Nome de canal inválido\n");
        return;
    }

    snprintf(clean, sizeof(clean), "#%s", channel_arg);

    // Se já estava noutro canal, avisa o canal antigo.
    // Não avisa saída se o user fizer join ao mesmo canal outra vez.
    if (strlen(clients[idx].current_channel) > 0 &&
        strcmp(clients[idx].current_channel, clean) != 0) {

        char old_msg[200];
        snprintf(old_msg, sizeof(old_msg),
                 "[SERVER] %s saiu do canal %s\n",
                 clients[idx].username,
                 clients[idx].current_channel);

        broadcast_to_channel(clients[idx].current_channel, old_msg);
    }

    snprintf(clients[idx].current_channel,
             sizeof(clients[idx].current_channel),
             "%s",
             clean);

    char response[200];
    snprintf(response, sizeof(response),
             "[SERVER] Entraste no canal %s\n",
             clients[idx].current_channel);

    send_response(clients[idx].fd, response);

    char join_msg[200];
    snprintf(join_msg, sizeof(join_msg),
             "[SERVER] %s entrou no canal %s\n",
             clients[idx].username,
             clients[idx].current_channel);

    broadcast_to_channel(clients[idx].current_channel, join_msg);
}

void handle_realtime_message(int idx, const char *msg) {
    if (!clients[idx].logged_in) {
        send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        return;
    }

    if (clients[idx].current_channel[0] == '\0') {
        send_response(clients[idx].fd, "Entra primeiro num canal com /join #general\n");
        return;
    }

    if (msg == NULL || msg[0] == '\0') {
        send_response(clients[idx].fd, "Mensagem vazia\n");
        return;
    }

    char out[BUF_SIZE + 150];
    snprintf(out, sizeof(out), "[%s] %s: %s\n", clients[idx].current_channel, clients[idx].username, msg);
    broadcast_to_channel(clients[idx].current_channel, out);
}

void handle_who(int idx) {
    if (!clients[idx].logged_in || clients[idx].current_channel[0] == '\0') {
        send_response(clients[idx].fd, "Entra primeiro num canal com /join #general\n");
        return;
    }

    char response[BUF_SIZE] = "Users neste canal:\n";
    int count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].logged_in && strcmp(clients[i].current_channel, clients[idx].current_channel) == 0) {
            if (strlen(response) + strlen(clients[i].username) + 4 < BUF_SIZE) {
                strcat(response, "- ");
                strcat(response, clients[i].username);
                strcat(response, "\n");
            }
            count++;
        }
    }

    if (count == 0) strcpy(response, "Sem users no canal\n");
    send_response(clients[idx].fd, response);
}

void handle_channels(int idx) {
    char response[BUF_SIZE] = "Canais ativos:\n";
    char seen[MAX_CLIENTS][MAX_CHANNEL];
    int seen_count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].logged_in && clients[i].current_channel[0] != '\0') {
            int exists = 0;
            for (int j = 0; j < seen_count; j++) {
                if (strcmp(seen[j], clients[i].current_channel) == 0) exists = 1;
            }

            if (!exists && seen_count < MAX_CLIENTS) {
                snprintf(seen[seen_count], MAX_CHANNEL, "%s", clients[i].current_channel);
                seen_count++;
            }
        }
    }

    if (seen_count == 0) {
        strcat(response, "- #general\n- #linux\n- #games\n");
    } else {
        for (int i = 0; i < seen_count; i++) {
            if (strlen(response) + strlen(seen[i]) + 4 < BUF_SIZE) {
                strcat(response, "- ");
                strcat(response, seen[i]);
                strcat(response, "\n");
            }
        }
    }

    send_response(clients[idx].fd, response);
}

void handle_get_peer(int idx, const char *target_user, const char *filename) {
    if (!clients[idx].logged_in) {
        send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        return;
    }

    if (!target_user || !filename || target_user[0] == '\0' || filename[0] == '\0') {
        send_response(clients[idx].fd, "Uso: /sendfile user ficheiro\n");
        return;
    }

    int target_idx = find_online_user(target_user);
    if (target_idx == -1) {
        send_response(clients[idx].fd, "USER_OFFLINE\n");
        return;
    }

    if (clients[target_idx].udp_port <= 0) {
        send_response(clients[idx].fd, "USER_HAS_NO_UDP_PORT\n");
        return;
    }

    char response[300];
    snprintf(response, sizeof(response), "PEER %s %s %d %s\n",
             clients[target_idx].username,
             clients[target_idx].ip,
             clients[target_idx].udp_port,
             filename);
    send_response(clients[idx].fd, response);

    char notify[300];
    snprintf(notify, sizeof(notify), "[SERVER] %s vai tentar enviar-te o ficheiro %s por UDP\n",
             clients[idx].username, filename);
    send_response(clients[target_idx].fd, notify);
}

void process_command(int idx, char *buffer, time_t start_time) {
    trim_newline(buffer);
    if (buffer[0] == '\0') return;

    char command[50];
    if (sscanf(buffer, "%49s", command) != 1) {
        send_response(clients[idx].fd, "INVALID_COMMAND\n");
        return;
    }

    if (strcmp(command, "REGISTER") == 0) {
        char username[50], password[50];

        if (sscanf(buffer, "REGISTER %49s %49s", username, password) != 2) {
            send_response(clients[idx].fd, "INVALID_FORMAT\n");
        } else if (strcmp(username, "admin") == 0) {
            send_response(clients[idx].fd, "RESERVED_USER\n");
        } else if (user_exists(username)) {
            send_response(clients[idx].fd, "USER_EXISTS\n");
        } else {
            send_response(clients[idx].fd, register_user(username, password) ? "REGISTERED\n" : "REGISTER_ERROR\n");
        }
    }
    else if (strcmp(command, "LOGIN") == 0) {
        char username[50], password[50];

        if (sscanf(buffer, "LOGIN %49s %49s", username, password) != 2) {
            send_response(clients[idx].fd, "INVALID_FORMAT\n");
        } else {
            int result = check_login(username, password);

            if (result == 1) {
                snprintf(clients[idx].username, sizeof(clients[idx].username), "%s", username);
                clients[idx].logged_in = 1;
                clients[idx].current_channel[0] = '\0';
                send_response(clients[idx].fd, "LOGIN_OK\n");
                printf("Login OK: %s fd=%d ip=%s\n", username, clients[idx].fd, clients[idx].ip);
            } else if (result == 2) {
                send_response(clients[idx].fd, "NOT_APPROVED\n");
            } else {
                send_response(clients[idx].fd, "LOGIN_FAIL\n");
            }
        }
    }
    else if (strcmp(command, "ECHO") == 0) {
        char *msg = buffer + 4;
        while (*msg == ' ') msg++;
        char response[BUF_SIZE];
        snprintf(response, sizeof(response), "%s\n", msg);
        send_response(clients[idx].fd, response);
    }
    else if (strcmp(command, "GET_INFO") == 0 || strcmp(command, "/get_info") == 0) {
        time_t uptime = time(NULL) - start_time;
        char info[300];
        snprintf(info, sizeof(info), "%s | Uptime: %ld seconds | Clientes ligados: ", SERVER_VERSION, uptime);
        int connected = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd != -1) connected++;

        char final[350];
        snprintf(final, sizeof(final), "%s%d\n", info, connected);
        send_response(clients[idx].fd, final);
    }
    else if (strcmp(command, "LIST_ALL") == 0) {
        char username[50], password[50];

        if (sscanf(buffer, "LIST_ALL %49s %49s", username, password) == 2) {
            if (check_login(username, password) != 1) send_response(clients[idx].fd, "AUTH_FAIL\n");
            else list_all_users(clients[idx].fd, is_admin_client(idx));
        } else if (clients[idx].logged_in) {
            list_all_users(clients[idx].fd, is_admin_client(idx));
        } else {
            send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        }
    }
    else if (strcmp(command, "/list_all") == 0) {
        if (!clients[idx].logged_in) send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        else list_all_users(clients[idx].fd, is_admin_client(idx));
    }
    else if (strcmp(command, "SEND") == 0) {
        char sender[50], password[50], receiver[50], message[900];

        if (sscanf(buffer, "SEND %49s %49s %49s %899[^\n]", sender, password, receiver, message) != 4) {
            send_response(clients[idx].fd, "INVALID_FORMAT\n");
        } else if (check_login(sender, password) != 1) {
            send_response(clients[idx].fd, "AUTH_FAIL\n");
        } else {
            store_message(clients[idx].fd, sender, receiver, message);
        }
    }
    else if (strcmp(command, "/send") == 0) {
        char receiver[50], message[900];
        if (!clients[idx].logged_in) {
            send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        } else if (sscanf(buffer, "/send %49s %899[^\n]", receiver, message) != 2) {
            send_response(clients[idx].fd, "Uso: /send user mensagem\n");
        } else {
            store_message(clients[idx].fd, clients[idx].username, receiver, message);
        }
    }
    else if (strcmp(command, "CHECK_INBOX") == 0) {
        char username[50], password[50];

        if (sscanf(buffer, "CHECK_INBOX %49s %49s", username, password) == 2) {
            if (check_login(username, password) != 1) send_response(clients[idx].fd, "AUTH_FAIL\n");
            else check_inbox(clients[idx].fd, username);
        } else if (clients[idx].logged_in) {
            check_inbox(clients[idx].fd, clients[idx].username);
        } else {
            send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        }
    }
    else if (strcmp(command, "/check_inbox") == 0) {
        if (!clients[idx].logged_in) send_response(clients[idx].fd, "AUTH_REQUIRED\n");
        else check_inbox(clients[idx].fd, clients[idx].username);
    }
    else if (strcmp(command, "APPROVE_USER") == 0) {
        char username[50], password[50], target_user[50];

        if (sscanf(buffer, "APPROVE_USER %49s %49s %49s", username, password, target_user) != 3) {
            send_response(clients[idx].fd, "INVALID_FORMAT\n");
        } else if (!is_admin_credentials(username, password)) {
            send_response(clients[idx].fd, "NOT_ADMIN\n");
        } else {
            approve_user(clients[idx].fd, target_user);
        }
    }
    else if (strcmp(command, "/approve") == 0) {
        char target_user[50];
        if (!is_admin_client(idx)) send_response(clients[idx].fd, "NOT_ADMIN\n");
        else if (sscanf(buffer, "/approve %49s", target_user) != 1) send_response(clients[idx].fd, "Uso: /approve user\n");
        else approve_user(clients[idx].fd, target_user);
    }
    else if (strcmp(command, "DELETE_USER") == 0) {
        char username[50], password[50], target_user[50];

        if (sscanf(buffer, "DELETE_USER %49s %49s %49s", username, password, target_user) != 3) {
            send_response(clients[idx].fd, "INVALID_FORMAT\n");
        } else if (!is_admin_credentials(username, password)) {
            send_response(clients[idx].fd, "NOT_ADMIN\n");
        } else {
            delete_user(clients[idx].fd, target_user);
        }
    }
    else if (strcmp(command, "/delete") == 0) {
        char target_user[50];
        if (!is_admin_client(idx)) send_response(clients[idx].fd, "NOT_ADMIN\n");
        else if (sscanf(buffer, "/delete %49s", target_user) != 1) send_response(clients[idx].fd, "Uso: /delete user\n");
        else delete_user(clients[idx].fd, target_user);
    }
    else if (strcmp(command, "/join") == 0) {
        char channel[50];
        if (sscanf(buffer, "/join %49s", channel) != 1) send_response(clients[idx].fd, "Uso: /join #canal\n");
        else handle_join(idx, channel);
    }
    else if (strcmp(command, "/msg") == 0) {
        char *msg = buffer + 4;
        while (*msg == ' ') msg++;
        handle_realtime_message(idx, msg);
    }
    else if (strcmp(command, "/who") == 0) {
        handle_who(idx);
    }
    else if (strcmp(command, "/channels") == 0) {
        handle_channels(idx);
    }
    else if (strcmp(command, "/udp_port") == 0 || strcmp(command, "UDP_PORT") == 0) {
        int port;
        if (sscanf(buffer, "%*s %d", &port) != 1 || port <= 0) {
            send_response(clients[idx].fd, "Uso: /udp_port porta\n");
        } else {
            clients[idx].udp_port = port;
            char response[100];
            snprintf(response, sizeof(response), "UDP_PORT_OK %d\n", clients[idx].udp_port);
            send_response(clients[idx].fd, response);
        }
    }
    else if (strcmp(command, "/sendfile") == 0 || strcmp(command, "GET_PEER") == 0) {
        char target_user[50], filename[200];
        if (sscanf(buffer, "%*s %49s %199s", target_user, filename) != 2) {
            send_response(clients[idx].fd, "Uso: /sendfile user ficheiro\n");
        } else {
            handle_get_peer(idx, target_user, filename);
        }
    }
    else if (strcmp(command, "/logout") == 0 || strcmp(command, "LOGOUT") == 0) {
        send_response(clients[idx].fd, "LOGOUT_OK\n");
        remove_client(idx);
    }
    else if (command[0] == '/') {
        send_response(clients[idx].fd, "UNKNOWN_COMMAND\n");
    }
    else {
        // Qualquer texto normal vira mensagem em tempo real para o canal atual
        handle_realtime_message(idx, buffer);
    }
}

int main(void) {
    time_t start_time = time(NULL);
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    ensure_data_folder();
    init_clients();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Erro no listen");
        close(server_fd);
        return 1;
    }

    printf("Servidor Phase 3 à escuta na porta %d...\n", PORT);
    printf("Usa vários terminais com ./client_phase3 para testar.\n");

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("Erro no select");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            client_len = sizeof(client_addr);
            int new_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (new_fd < 0) {
                perror("Erro no accept");
            } else {
                int slot = find_free_client_slot();
                if (slot == -1) {
                    send_response(new_fd, "SERVER_FULL\n");
                    close(new_fd);
                } else {
                    clients[slot].fd = new_fd;
                    clients[slot].logged_in = 0;
                    clients[slot].username[0] = '\0';
                    clients[slot].current_channel[0] = '\0';
                    clients[slot].udp_port = 0;
                    inet_ntop(AF_INET, &client_addr.sin_addr, clients[slot].ip, sizeof(clients[slot].ip));

                    printf("Novo cliente fd=%d ip=%s slot=%d\n", new_fd, clients[slot].ip, slot);
                    send_response(new_fd, "CONNECTED\n");
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &read_fds)) {
                char buffer[BUF_SIZE];
                int n = recv(clients[i].fd, buffer, sizeof(buffer) - 1, 0);

                if (n <= 0) {
                    remove_client(i);
                } else {
                    buffer[n] = '\0';
                    process_command(i, buffer, start_time);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
