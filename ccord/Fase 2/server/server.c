#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h> // usado para calcular uptime

#define PORT 9000
#define BUF_SIZE 1024

// ---------- VERIFICAR SE USER EXISTE ----------
int user_exists(char *username) {
    FILE *file = fopen("../data/users.txt", "r");
    if (!file) return 0;

    char user[100], pass[100];
    int status; // F5

    while (fscanf(file, "%s %s %d", user, pass, &status) != EOF) {
        if (strcmp(user, username) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

// ---------- REGISTAR USER ----------
void register_user(char *username, char *password) {
    FILE *file = fopen("../data/users.txt", "a");
    fprintf(file, "%s %s %d\n", username, password, 0); // F5
    fclose(file);
}

// ---------- LOGIN ----------
int check_login(char *username, char *password) {
    FILE *file = fopen("../data/users.txt", "r");
    if (!file) return 0;

    char user[100], pass[100];
    int status;

    while (fscanf(file, "%s %s %d", user, pass, &status) != EOF) {
        if (strcmp(user, username) == 0 && strcmp(pass, password) == 0) {

            // F6: permitir admin sempre
            if (strcmp(username, "admin") == 0) {
                fclose(file);
                return 1;
            }

            fclose(file);

            if (status == 1)
                return 1;
            else
                return 2;
        }
    }

    fclose(file);
    return 0;
}

// ---------- MAIN ----------
int main() {

    time_t start_time = time(NULL);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    char buffer[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    printf("Servidor à escuta na porta %d...\n", PORT);

    client_len = sizeof(client_addr);

    while (1) {

        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);

        if (n > 0) {
            buffer[n] = '\0';

            char command[50], username[50], password[50], message[900];

            sscanf(buffer, "%s %s %s %[^\n]", command, username, password, message);

            // ---------- REGISTER ----------
            if (strcmp(command, "REGISTER") == 0) {
                if (user_exists(username))
                    send(client_fd, "USER_EXISTS\n", 12, 0);
                else {
                    register_user(username, password);
                    send(client_fd, "REGISTERED\n", 11, 0);
                }
            }

            // ---------- LOGIN ----------
            else if (strcmp(command, "LOGIN") == 0) {
                int result = check_login(username, password);

                if (result == 1)
                    send(client_fd, "LOGIN_OK\n", 9, 0);
                else if (result == 2)
                    send(client_fd, "NOT_APPROVED\n", 13, 0);
                else
                    send(client_fd, "LOGIN_FAIL\n", 11, 0);
            }

            // ---------- ECHO ----------
            else if (strcmp(command, "ECHO") == 0) {
                char *msg = buffer + 5;
                send(client_fd, msg, strlen(msg), 0);
            }

            // ---------- GET_INFO ----------
            else if (strcmp(command, "GET_INFO") == 0) {
                time_t uptime = time(NULL) - start_time;

                char info[200];
                snprintf(info, sizeof(info),
                    "Server v1.0 - Running OK | Uptime: %ld seconds\n",
                    uptime);

                send(client_fd, info, strlen(info), 0);
            }

            // --------- F4: LIST_ALL ---------
            else if (strcmp(command, "LIST_ALL") == 0) {

                FILE *file = fopen("../data/users.txt", "r");
                if (!file) {
                    send(client_fd, "Erro ao abrir ficheiro\n", 24, 0);
                    close(client_fd);
                    continue;
                }

                char user[100], pass[100];
                int status;
                char response[BUF_SIZE] = "";

                while (fscanf(file, "%s %s %d", user, pass, &status) != EOF) {
                    
                    // Mostrar apenas utilizadores aprovados (status == 1)
                    if (status == 1) {
                        strncat(response, user, BUF_SIZE - strlen(response) - 1);
                        strncat(response, "\n", BUF_SIZE - strlen(response) - 1);
                    }
                }

                fclose(file);

                if (strlen(response) == 0) {
                    strcpy(response, "Sem utilizadores aprovados\n");
                }

                send(client_fd, response, strlen(response), 0);
            }

            // --------- F4: SEND MESSAGE ---------
            else if (strcmp(command, "SEND") == 0) {

                FILE *file = fopen("../data/messages.txt", "a");

                fprintf(file, "%s|%s|%s\n", username, password, message);

                fclose(file);

                send(client_fd, "MSG_STORED\n", 11, 0);
            }

            // --------- F4: CHECK_INBOX ---------
            else if (strcmp(command, "CHECK_INBOX") == 0) {

                FILE *file = fopen("../data/messages.txt", "r");

                char line[1000];
                char response[BUF_SIZE] = "";

                while (fgets(line, sizeof(line), file)) {

                    char sender[50], receiver[50], msg[900];

                    sscanf(line, "%[^|]|%[^|]|%[^\n]", sender, receiver, msg);

                    if (strcmp(receiver, username) == 0) {
                        strcat(response, sender);
                        strcat(response, ": ");
                        strcat(response, msg);
                        strcat(response, "\n");
                    }
                }

                fclose(file);

                if (strlen(response) == 0)
                    strcpy(response, "Inbox vazia\n");

                send(client_fd, response, strlen(response), 0);
            }

            // --------- F6: APPROVE USER ---------
            else if (strcmp(command, "APPROVE_USER") == 0) {

                if (strcmp(username, "admin") != 0) { // F6
                    send(client_fd, "NOT_ADMIN\n", 10, 0);
                    close(client_fd);
                    continue;
                }

                FILE *file = fopen("../data/users.txt", "r");
                FILE *temp = fopen("../data/temp.txt", "w");

                char user[100], pass[100];
                int status;
                int found = 0;

                while (fscanf(file, "%s %s %d", user, pass, &status) != EOF) {

                    if (strcmp(user, password) == 0) { // F6
                        status = 1;
                        found = 1;
                    }

                    fprintf(temp, "%s %s %d\n", user, pass, status);
                }

                fclose(file);
                fclose(temp);

                remove("../data/users.txt");
                rename("../data/temp.txt", "../data/users.txt");

                if (found)
                    send(client_fd, "USER_APPROVED\n", 14, 0);
                else
                    send(client_fd, "USER_NOT_FOUND\n", 16, 0);
            }

            // --------- F6: DELETE USER ---------
            else if (strcmp(command, "DELETE_USER") == 0) {

                if (strcmp(username, "admin") != 0) { // F6
                    send(client_fd, "NOT_ADMIN\n", 10, 0);
                    close(client_fd);
                    continue;
                }

                FILE *file = fopen("../data/users.txt", "r");
                FILE *temp = fopen("../data/temp.txt", "w");

                char user[100], pass[100];
                int status;
                int found = 0;

                while (fscanf(file, "%s %s %d", user, pass, &status) != EOF) {

                    if (strcmp(user, password) == 0) {
                        found = 1;
                        continue;
                    }

                    fprintf(temp, "%s %s %d\n", user, pass, status);
                }

                fclose(file);
                fclose(temp);

                remove("../data/users.txt");
                rename("../data/temp.txt", "../data/users.txt");

                if (found)
                    send(client_fd, "USER_DELETED\n", 13, 0);
                else
                    send(client_fd, "USER_NOT_FOUND\n", 16, 0);
            }

            else {
                send(client_fd, "UNKNOWN_COMMAND\n", 17, 0);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
