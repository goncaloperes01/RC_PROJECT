#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF_SIZE 1024

// Cores ANSI usadas para melhorar a experiência do utilizador no terminal
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

// Função auxiliar que limpa o ecrã do terminal
// (não afeta lógica de rede, apenas interface)
void clear_screen() {
    system("clear");
}

// Função para pausar a execução até o utilizador carregar ENTER
// usada para melhorar a navegação entre menus
void pause_screen() {
    printf("\nPressiona ENTER para continuar...");
    getchar();
}

int main() {

    int sock; // descritor do socket usado para comunicar com o servidor
    struct sockaddr_in server_addr; // estrutura que define IP e porta do servidor
    char buffer[BUF_SIZE]; // buffer onde são armazenadas mensagens enviadas/recebidas

    int logged_in = 0; // variável de estado que indica se o utilizador está autenticado
    char current_user[50]; // guarda o username após login bem sucedido

    // ciclo principal do cliente (permite múltiplas operações)
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

            sock = socket(AF_INET, SOCK_STREAM, 0);

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

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
                else if (strstr(buffer, "NOT_APPROVED")) { // F5
                    printf(YELLOW "⏳ Conta ainda não foi aprovada pelo admin!\n" RESET);
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

            // --------- F4: Novas opções ---------
            printf(" [4] LIST_ALL\n");
            printf(" [5] SEND\n");
            printf(" [6] CHECK_INBOX\n");

            // --------- F6: Novas opções ---------
            printf(" [7] APPROVE USER\n"); // F6
            printf(" [8] DELETE USER\n");  // F6

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
            else if (option == 4) {
                sprintf(buffer, "LIST_ALL");
            }
            else if (option == 5) {
                char dest[50];
                char msg[900];

                printf("Enviar para: ");
                fgets(dest, 50, stdin);
                dest[strcspn(dest, "\n")] = 0;

                printf("Mensagem: ");
                fgets(msg, 900, stdin);
                msg[strcspn(msg, "\n")] = 0;

                sprintf(buffer, "SEND %s %s %s", current_user, dest, msg);
            }
            else if (option == 6) {
                sprintf(buffer, "CHECK_INBOX %s", current_user);
            }

            // --------- F6: APPROVE USER ---------
            else if (option == 7) {
                char user[50];

                printf("User a aprovar: ");
                fgets(user, 50, stdin);
                user[strcspn(user, "\n")] = 0;

                sprintf(buffer, "APPROVE_USER %s %s", current_user, user);
            }

            // --------- F6: DELETE USER ---------
            else if (option == 8) {
                char user[50];

                printf("User a apagar: ");
                fgets(user, 50, stdin);
                user[strcspn(user, "\n")] = 0;

                sprintf(buffer, "DELETE_USER %s %s", current_user, user);
            }

            else {
                printf(RED "Opção inválida!\n" RESET);
                close(sock);
                pause_screen();
                continue;
            }

            send(sock, buffer, strlen(buffer), 0);

            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';

                // --------- F6: respostas ---------
                if (strstr(buffer, "NOT_ADMIN")) {
                    printf(RED "✖ Apenas o admin pode fazer isso!\n" RESET);
                }
                else if (strstr(buffer, "USER_APPROVED")) {
                    printf(GREEN "✔ Utilizador aprovado!\n" RESET);
                }
                else if (strstr(buffer, "USER_DELETED")) {
                    printf(GREEN "✔ Utilizador apagado!\n" RESET);
                }
                else if (strstr(buffer, "USER_NOT_FOUND")) {
                    printf(RED "✖ Utilizador não encontrado!\n" RESET);
                }
                else {
                    printf(GREEN "\nResposta do servidor:\n%s\n" RESET, buffer);
                }
            }

            close(sock);
            pause_screen();
        }
    }

    return 0;
}
