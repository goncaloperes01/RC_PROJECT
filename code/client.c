#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    int logged_in = 0;
    char current_user[50];

    while (1) {

        // ---------- MENU INICIAL ----------
        if (!logged_in) {
            int option;
            char username[50], password[50];

            printf("\n=== C-CORD ===\n");
            printf("1. Register\n");
            printf("2. Login\n");
            printf("3. Exit\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar(); // limpar buffer

            if (option == 3) {
                printf("Adeus!\n");
                break;
            }

            printf("Username: ");
            fgets(username, 50, stdin);
            username[strcspn(username, "\n")] = 0;

            printf("Password: ");
            fgets(password, 50, stdin);
            password[strcspn(password, "\n")] = 0;

            // Criar socket
            sock = socket(AF_INET, SOCK_STREAM, 0);

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

            // Criar comando
            if (option == 1) {
                sprintf(buffer, "REGISTER %s %s", username, password);
            } else if (option == 2) {
                sprintf(buffer, "LOGIN %s %s", username, password);
            } else {
                printf("Opção inválida\n");
                continue;
            }

            // Enviar
            send(sock, buffer, strlen(buffer), 0);

            // Receber
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Servidor: %s", buffer);

                // Se login OK → entra na sessão
                if (strstr(buffer, "LOGIN_OK")) {
                    logged_in = 1;
                    strcpy(current_user, username);
                }
            }

            close(sock);
        }

        // ---------- MENU APÓS LOGIN ----------
        else {
            int option;

            printf("\n=== Bem-vindo %s ===\n", current_user);
            printf("1. ECHO\n");
            printf("2. GET_INFO\n");
            printf("3. Logout\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar();

            if (option == 3) {
                logged_in = 0;
                printf("Logout feito.\n");
                continue;
            }

            // Criar socket
            sock = socket(AF_INET, SOCK_STREAM, 0);

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

            // ---------- ECHO ----------
            if (option == 1) {
                char msg[900];

                printf("Mensagem: ");
                fgets(msg, 900, stdin);
                msg[strcspn(msg, "\n")] = 0;

                sprintf(buffer, "ECHO %s", msg);
            }

            // ---------- GET_INFO ----------
            else if (option == 2) {
                sprintf(buffer, "GET_INFO");
            }

            else {
                printf("Opção inválida\n");
                close(sock);
                continue;
            }

            // Enviar
            send(sock, buffer, strlen(buffer), 0);

            // Receber
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Servidor: %s\n", buffer);
            }

            close(sock);
        }
    }

    return 0;
}
