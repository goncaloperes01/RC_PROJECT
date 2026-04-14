#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF_SIZE 1024

// Cores
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

void clear_screen() {
    system("clear");
}

void pause_screen() {
    printf("\nPressiona ENTER para continuar...");
    getchar();
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    int logged_in = 0;
    char current_user[50];

    while (1) {

        // ---------- MENU INICIAL ----------
        if (!logged_in) {
            clear_screen();

            int option;
            char username[50], password[50];

            printf(CYAN "\n=========================\n");
            printf("        C-CORD v1.0\n");
            printf("=========================\n" RESET);
            printf(" [1] Register\n");
            printf(" [2] Login\n");
            printf(" [3] Exit\n");
            printf("=========================\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar();

            if (option == 3) {
                printf(YELLOW "A sair...\n" RESET);
                break;
            }

            printf("\nUsername: ");
            fgets(username, 50, stdin);
            username[strcspn(username, "\n")] = 0;

            printf("Password: ");
            fgets(password, 50, stdin);
            password[strcspn(password, "\n")] = 0;

            printf("\nA processar...\n");
            sleep(1);

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
                printf(RED "Opção inválida!\n" RESET);
                pause_screen();
                continue;
            }

            send(sock, buffer, strlen(buffer), 0);

            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';

                if (strstr(buffer, "REGISTERED")) {
                    printf(GREEN "✔ Registo feito com sucesso!\n" RESET);
                }
                else if (strstr(buffer, "USER_EXISTS")) {
                    printf(RED "✖ Utilizador já existe!\n" RESET);
                }
                else if (strstr(buffer, "LOGIN_OK")) {
                    printf(GREEN "✔ Login bem sucedido!\n" RESET);
                    logged_in = 1;
                    strcpy(current_user, username);
                }
                else if (strstr(buffer, "LOGIN_FAIL")) {
                    printf(RED "✖ Credenciais erradas!\n" RESET);
                }
                else {
                    printf("Servidor: %s\n", buffer);
                }
            }

            close(sock);
            pause_screen();
        }

        // ---------- MENU APÓS LOGIN ----------
        else {
            clear_screen();

            int option;

            printf(CYAN "\n=========================\n");
            printf("     Bem-vindo %s\n", current_user);
            printf("=========================\n" RESET);
            printf(" [1] ECHO (mensagem)\n");
            printf(" [2] GET_INFO\n");
            printf(" [3] Logout\n");
            printf("=========================\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar();

            if (option == 3) {
                printf(YELLOW "Logout feito.\n" RESET);
                logged_in = 0;
                pause_screen();
                continue;
            }

            sock = socket(AF_INET, SOCK_STREAM, 0);

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

            if (option == 1) {
                char msg[900];

                printf("\nMensagem: ");
                fgets(msg, 900, stdin);
                msg[strcspn(msg, "\n")] = 0;

                sprintf(buffer, "ECHO %s", msg);
            }
            else if (option == 2) {
                sprintf(buffer, "GET_INFO");
            }
            else {
                printf(RED "Opção inválida!\n" RESET);
                close(sock);
                pause_screen();
                continue;
            }

            printf("\nA enviar...\n");
            sleep(1);

            send(sock, buffer, strlen(buffer), 0);

            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf(GREEN "\nResposta do servidor:\n%s\n" RESET, buffer);
            }

            close(sock);
            pause_screen();
        }
    }

    return 0;
}