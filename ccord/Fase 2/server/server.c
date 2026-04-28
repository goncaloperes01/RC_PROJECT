#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 9000
#define BUF_SIZE 1024

#define USERS_FILE "../data/users.txt"
#define MESSAGES_FILE "../data/messages.txt"

// Envia uma resposta ao cliente através do socket
void send_response(int client_fd, const char *msg) {
    send(client_fd, msg, strlen(msg), 0);
}

// Lê uma linha do ficheiro users.txt.
// Suporta o formato novo: username password status
// E também o formato antigo: username password
int parse_user_line(char *line, char *user, char *pass, int *status) {
    int fields = sscanf(line, "%99s %99s %d", user, pass, status);

    if (fields == 3) {
        return 1;
    }

    if (fields == 2) {
        // Se não existir status no ficheiro, assume-se que o utilizador já está aprovado
        *status = 1;
        return 1;
    }

    return 0;
}

// Verifica se um username já existe no ficheiro de utilizadores
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

// Regista um novo utilizador no ficheiro users.txt.
// O status fica a 0, ou seja, fica pendente até ser aprovado pelo admin.
int register_user(const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE, "a");
    if (!file) return 0;

    fprintf(file, "%s %s %d\n", username, password, 0);

    fclose(file);
    return 1;
}

// Verifica as credenciais de login.
// return 1 -> login correto e user aprovado
// return 2 -> password correta, mas user ainda não aprovado
// return 0 -> login falhou
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

                if (status == 1)
                    return 1;
                else
                    return 2;
            }
        }
    }

    fclose(file);
    return 0;
}

// Verifica se o utilizador autenticado é o admin
int is_admin(const char *username, const char *password) {
    return strcmp(username, "admin") == 0 && check_login(username, password) == 1;
}

// Lista todos os utilizadores aprovados, ou seja, com status = 1
void list_all_users(int client_fd) {
    FILE *file = fopen(USERS_FILE, "r");

    if (!file) {
        send_response(client_fd, "Erro ao abrir users.txt\n");
        return;
    }

    char line[300];
    char user[100], pass[100];
    int status;

    char response[BUF_SIZE] = "";

    while (fgets(line, sizeof(line), file)) {
        if (parse_user_line(line, user, pass, &status)) {
            if (status == 1) {
                // Junta cada username aprovado à resposta final
                strncat(response, user, BUF_SIZE - strlen(response) - 1);
                strncat(response, "\n", BUF_SIZE - strlen(response) - 1);
            }
        }
    }

    fclose(file);

    if (strlen(response) == 0) {
        strcpy(response, "Sem utilizadores aprovados\n");
    }

    send_response(client_fd, response);
}

// Guarda uma mensagem privada no ficheiro messages.txt
// Formato usado: sender|receiver|message
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

// Procura no ficheiro messages.txt as mensagens recebidas pelo utilizador
void check_inbox(int client_fd, const char *username) {
    FILE *file = fopen(MESSAGES_FILE, "r");

    if (!file) {
        send_response(client_fd, "Inbox vazia\n");
        return;
    }

    char line[1200];
    char response[BUF_SIZE] = "";

    while (fgets(line, sizeof(line), file)) {
        char sender[50], receiver[50], msg[900];

        // Lê uma mensagem no formato sender|receiver|message
        if (sscanf(line, "%49[^|]|%49[^|]|%899[^\n]", sender, receiver, msg) == 3) {
            if (strcmp(receiver, username) == 0) {
                char entry[1000];

                snprintf(entry, sizeof(entry), "%s: %s\n", sender, msg);

                // Só acrescenta se ainda houver espaço no buffer de resposta
                if (strlen(response) + strlen(entry) < BUF_SIZE - 1) {
                    strncat(response, entry, BUF_SIZE - strlen(response) - 1);
                }
            }
        }
    }

    fclose(file);

    if (strlen(response) == 0) {
        strcpy(response, "Inbox vazia\n");
    }

    send_response(client_fd, response);
}

// Aprova um utilizador pendente, mudando o seu status para 1
void approve_user(int client_fd, const char *target_user) {
    FILE *file = fopen(USERS_FILE, "r");
    FILE *temp = fopen("../data/temp.txt", "w");

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

            // Escreve todos os users para um ficheiro temporário, já com a alteração feita
            fprintf(temp, "%s %s %d\n", user, pass, status);
        }
    }

    fclose(file);
    fclose(temp);

    // Substitui o ficheiro antigo pelo ficheiro atualizado
    remove(USERS_FILE);
    rename("../data/temp.txt", USERS_FILE);

    if (found)
        send_response(client_fd, "USER_APPROVED\n");
    else
        send_response(client_fd, "USER_NOT_FOUND\n");
}

// Apaga um utilizador do ficheiro, exceto o admin
void delete_user(int client_fd, const char *target_user) {
    if (strcmp(target_user, "admin") == 0) {
        send_response(client_fd, "CANNOT_DELETE_ADMIN\n");
        return;
    }

    FILE *file = fopen(USERS_FILE, "r");
    FILE *temp = fopen("../data/temp.txt", "w");

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
                continue; // Não copia este user para o ficheiro temporário
            }

            fprintf(temp, "%s %s %d\n", user, pass, status);
        }
    }

    fclose(file);
    fclose(temp);

    remove(USERS_FILE);
    rename("../data/temp.txt", USERS_FILE);

    if (found)
        send_response(client_fd, "USER_DELETED\n");
    else
        send_response(client_fd, "USER_NOT_FOUND\n");
}

int main() {
    time_t start_time = time(NULL);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    char buffer[BUF_SIZE];

    // Cria o socket TCP do servidor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    // Permite reutilizar a mesma porta depois de reiniciar o servidor
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configura o endereço e a porta do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Associa o socket à porta definida
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind");
        close(server_fd);
        return 1;
    }

    // Coloca o servidor à escuta de ligações
    if (listen(server_fd, 5) < 0) {
        perror("Erro no listen");
        close(server_fd);
        return 1;
    }

    printf("Servidor à escuta na porta %d...\n", PORT);

    // Ciclo principal do servidor: aceita um cliente, processa o pedido e fecha a ligação
    while (1) {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("Erro no accept");
            continue;
        }

        int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);

        if (n > 0) {
            buffer[n] = '\0';

            char command[50];

            // Lê a primeira palavra recebida para identificar o comando
            if (sscanf(buffer, "%49s", command) != 1) {
                send_response(client_fd, "INVALID_COMMAND\n");
                close(client_fd);
                continue;
            }

            // REGISTER username password
            if (strcmp(command, "REGISTER") == 0) {
                char username[50], password[50];

                if (sscanf(buffer, "REGISTER %49s %49s", username, password) != 2) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (strcmp(username, "admin") == 0) {
                    send_response(client_fd, "RESERVED_USER\n");
                }
                else if (user_exists(username)) {
                    send_response(client_fd, "USER_EXISTS\n");
                }
                else {
                    if (register_user(username, password))
                        send_response(client_fd, "REGISTERED\n");
                    else
                        send_response(client_fd, "REGISTER_ERROR\n");
                }
            }

            // LOGIN username password
            else if (strcmp(command, "LOGIN") == 0) {
                char username[50], password[50];

                if (sscanf(buffer, "LOGIN %49s %49s", username, password) != 2) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else {
                    int result = check_login(username, password);

                    if (result == 1)
                        send_response(client_fd, "LOGIN_OK\n");
                    else if (result == 2)
                        send_response(client_fd, "NOT_APPROVED\n");
                    else
                        send_response(client_fd, "LOGIN_FAIL\n");
                }
            }

            // ECHO mensagem
            else if (strcmp(command, "ECHO") == 0) {
                char *msg = buffer + 5;
                send_response(client_fd, msg);
            }

            // GET_INFO
            else if (strcmp(command, "GET_INFO") == 0) {
                time_t uptime = time(NULL) - start_time;

                char info[200];

                snprintf(info, sizeof(info),
                    "Server v1.0 - Running OK | Uptime: %ld seconds\n",
                    uptime);

                send_response(client_fd, info);
            }

            // LIST_ALL username password
            else if (strcmp(command, "LIST_ALL") == 0) {
                char username[50], password[50];

                if (sscanf(buffer, "LIST_ALL %49s %49s", username, password) != 2) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (check_login(username, password) != 1) {
                    send_response(client_fd, "AUTH_FAIL\n");
                }
                else {
                    list_all_users(client_fd);
                }
            }

            // SEND sender password receiver message
            else if (strcmp(command, "SEND") == 0) {
                char sender[50], password[50], receiver[50], message[900];

                // O último campo lê a mensagem completa até ao fim da linha, incluindo espaços
                if (sscanf(buffer, "SEND %49s %49s %49s %899[^\n]",
                           sender, password, receiver, message) != 4) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (check_login(sender, password) != 1) {
                    send_response(client_fd, "AUTH_FAIL\n");
                }
                else {
                    store_message(client_fd, sender, receiver, message);
                }
            }

            // CHECK_INBOX username password
            else if (strcmp(command, "CHECK_INBOX") == 0) {
                char username[50], password[50];

                if (sscanf(buffer, "CHECK_INBOX %49s %49s", username, password) != 2) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (check_login(username, password) != 1) {
                    send_response(client_fd, "AUTH_FAIL\n");
                }
                else {
                    check_inbox(client_fd, username);
                }
            }

            // APPROVE_USER admin password target_user
            else if (strcmp(command, "APPROVE_USER") == 0) {
                char username[50], password[50], target_user[50];

                if (sscanf(buffer, "APPROVE_USER %49s %49s %49s",
                           username, password, target_user) != 3) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (!is_admin(username, password)) {
                    send_response(client_fd, "NOT_ADMIN\n");
                }
                else {
                    approve_user(client_fd, target_user);
                }
            }

            // DELETE_USER admin password target_user
            else if (strcmp(command, "DELETE_USER") == 0) {
                char username[50], password[50], target_user[50];

                if (sscanf(buffer, "DELETE_USER %49s %49s %49s",
                           username, password, target_user) != 3) {
                    send_response(client_fd, "INVALID_FORMAT\n");
                }
                else if (!is_admin(username, password)) {
                    send_response(client_fd, "NOT_ADMIN\n");
                }
                else {
                    delete_user(client_fd, target_user);
                }
            }

            // Caso o comando recebido não exista
            else {
                send_response(client_fd, "UNKNOWN_COMMAND\n");
            }
        }

        // Fecha a ligação ao cliente atual
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
