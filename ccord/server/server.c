#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h> // usado para calcular uptime

#define PORT 9000
#define BUF_SIZE 1024

// ---------- VERIFICAR SE USER EXISTE ----------
// Verifica se um utilizador já está registado no ficheiro
int user_exists(char *username) {
    FILE *file = fopen("../data/users.txt", "r");
    if (!file) return 0; // se não conseguir abrir, assume que não existe

    char user[100], pass[100];

    // percorre todas as linhas (username + password)
    while (fscanf(file, "%s %s", user, pass) != EOF) {
        if (strcmp(user, username) == 0) {
            fclose(file);
            return 1; // utilizador encontrado
        }
    }

    fclose(file);
    return 0; // não encontrado
}

// ---------- REGISTAR USER ----------
// Adiciona um novo utilizador ao ficheiro
void register_user(char *username, char *password) {
    FILE *file = fopen("../data/users.txt", "a"); // modo append
    if (!file) {
        perror("Erro ao abrir ficheiro");
        return;
    }

    fprintf(file, "%s %s\n", username, password);
    fclose(file);
}

// ---------- LOGIN ----------
// Verifica credenciais (username + password)
int check_login(char *username, char *password) {
    FILE *file = fopen("../data/users.txt", "r");
    if (!file) return 0;

    char user[100], pass[100];

    while (fscanf(file, "%s %s", user, pass) != EOF) {
        if (strcmp(user, username) == 0 && strcmp(pass, password) == 0) {
            fclose(file);
            return 1; // login válido
        }
    }

    fclose(file);
    return 0; // login inválido
}

// ---------- MAIN ----------
int main() {

    // guarda o instante inicial do servidor (para calcular uptime)
    time_t start_time = time(NULL);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    char buffer[BUF_SIZE];

    // Criação de socket TCP (AF_INET + SOCK_STREAM)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    // Configuração do endereço do servidor
    server_addr.sin_family = AF_INET;            // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;    // aceita ligações de qualquer interface
    server_addr.sin_port = htons(PORT);          // conversão para network byte order

    // Associa o socket a um IP + porta
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind");
        exit(1);
    }

    // Coloca o socket em modo passivo (escuta conexões)
    if (listen(server_fd, 5) < 0) {
        perror("Erro no listen");
        exit(1);
    }

    printf("Servidor à escuta na porta %d...\n", PORT);

    client_len = sizeof(client_addr);

    // ciclo principal (servidor sequencial)
    while (1) {

        // aceita uma ligação de um cliente
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Erro no accept");
            continue;
        }

        printf("Cliente ligado!\n");

        // recebe mensagem do cliente
        int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);

        if (n > 0) {
            buffer[n] = '\0'; // garante string válida
            printf("Recebido: %s\n", buffer);

            char command[50], username[50], password[50], message[900];

            // separa comando e argumentos
            sscanf(buffer, "%s %s %s %[^\n]", command, username, password, message);

            // ---------- REGISTER ----------
            if (strcmp(command, "REGISTER") == 0) {

                if (user_exists(username)) {
                    send(client_fd, "USER_EXISTS\n", 12, 0);
                } else {
                    register_user(username, password);
                    send(client_fd, "REGISTERED\n", 11, 0);
                }

            // ---------- LOGIN ----------
            } else if (strcmp(command, "LOGIN") == 0) {

                if (check_login(username, password)) {
                    send(client_fd, "LOGIN_OK\n", 9, 0);
                } else {
                    send(client_fd, "LOGIN_FAIL\n", 11, 0);
                }

            // ---------- ECHO ----------
            } else if (strcmp(command, "ECHO") == 0) {

                // devolve a mensagem recebida (eco)
                char *msg = buffer + 5; // ignora "ECHO "
                send(client_fd, msg, strlen(msg), 0);

            // ---------- GET_INFO ----------
            } else if (strcmp(command, "GET_INFO") == 0) {

                // calcula há quanto tempo o servidor está ativo
                time_t uptime = time(NULL) - start_time;

                char info[200];
                snprintf(info, sizeof(info),
                    "Server v1.0 - Running OK | Uptime: %ld seconds\n",
                    uptime);

                send(client_fd, info, strlen(info), 0);

            // ---------- UNKNOWN ----------
            } else {
                // comando não reconhecido
                send(client_fd, "UNKNOWN_COMMAND\n", 17, 0);
            }
        }

        // fecha ligação (servidor trata um cliente de cada vez)
        close(client_fd);
        printf("Cliente desligado.\n");
    }

    close(server_fd);
    return 0;
}
