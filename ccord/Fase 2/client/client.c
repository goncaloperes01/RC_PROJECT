#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF_SIZE 1024

// Códigos ANSI para dar cor ao texto no terminal
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

// Limpa o ecrã do terminal
void clear_screen() {
    system("clear");
}

// Pausa o programa até o utilizador carregar ENTER
void pause_screen() {
    printf("\nPressiona ENTER para continuar...");
    getchar();
}

// Cria uma ligação TCP ao servidor local, na porta definida
int connect_to_server() {
    int sock;
    struct sockaddr_in server_addr;

    // Cria o socket TCP do cliente
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("Erro ao criar socket");
        return -1;
    }

    // Define o tipo de endereço e a porta do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Converte o IP 127.0.0.1 para o formato usado pelos sockets
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Erro no IP do servidor");
        close(sock);
        return -1;
    }

    // Tenta estabelecer ligação ao servidor
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao ligar ao servidor");
        close(sock);
        return -1;
    }

    return sock;
}

int main() {
    int sock;
    char buffer[BUF_SIZE];

    int logged_in = 0;
    char current_user[50] = "";
    char current_pass[50] = "";

    while (1) {

        // =====================================================
        // MENU INICIAL: REGISTER, LOGIN OU EXIT
        // =====================================================
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

            if (option != 1 && option != 2) {
                printf(RED "Opção inválida!\n" RESET);
                pause_screen();
                continue;
            }

            // Lê as credenciais introduzidas pelo utilizador
            printf("\nUsername: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = 0;

            printf("Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = 0;

            // Liga ao servidor para enviar o pedido escolhido
            sock = connect_to_server();

            if (sock < 0) {
                pause_screen();
                continue;
            }

            // Monta o comando a enviar ao servidor
            if (option == 1) {
                snprintf(buffer, sizeof(buffer), "REGISTER %s %s", username, password);
            }
            else {
                snprintf(buffer, sizeof(buffer), "LOGIN %s %s", username, password);
            }

            send(sock, buffer, strlen(buffer), 0);

            int n = recv(sock, buffer, BUF_SIZE - 1, 0);

            if (n > 0) {
                buffer[n] = '\0';

                // Interpreta a resposta enviada pelo servidor
                if (strstr(buffer, "REGISTERED")) {
                    printf(GREEN "✔ Registo feito com sucesso!\n" RESET);
                    printf(YELLOW "A tua conta ficou pendente. Precisa de aprovação do admin.\n" RESET);
                }
                else if (strstr(buffer, "USER_EXISTS")) {
                    printf(RED "✖ Utilizador já existe!\n" RESET);
                }
                else if (strstr(buffer, "RESERVED_USER")) {
                    printf(RED "✖ O username 'admin' está reservado!\n" RESET);
                }
                else if (strstr(buffer, "LOGIN_OK")) {
                    printf(GREEN "✔ Login bem sucedido!\n" RESET);

                    // Guarda o utilizador atual para pedidos futuros
                    logged_in = 1;
                    strcpy(current_user, username);
                    strcpy(current_pass, password);
                }
                else if (strstr(buffer, "NOT_APPROVED")) {
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

        // =====================================================
        // MENU APÓS LOGIN: FUNCIONALIDADES DO CLIENTE
        // =====================================================
        else {
            clear_screen();

            int option;

            printf(CYAN "\n=========================\n");
            printf("     Bem-vindo %s\n", current_user);
            printf("=========================\n" RESET);
            printf(" [1] ECHO\n");
            printf(" [2] GET_INFO\n");
            printf(" [3] Logout\n");
            printf(" [4] LIST_ALL\n");
            printf(" [5] SEND\n");
            printf(" [6] CHECK_INBOX\n");

            // As opções de administração só aparecem para o utilizador admin
            if (strcmp(current_user, "admin") == 0) {
                printf(" [7] APPROVE USER\n");
                printf(" [8] DELETE USER\n");
            }

            printf("=========================\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar();

            if (option == 3) {
                printf(YELLOW "Logout feito.\n" RESET);
                logged_in = 0;
                strcpy(current_user, "");
                strcpy(current_pass, "");
                pause_screen();
                continue;
            }

            // Cada opção abre uma nova ligação ao servidor
            sock = connect_to_server();

            if (sock < 0) {
                pause_screen();
                continue;
            }

            // ECHO mensagem
            if (option == 1) {
                char msg[900];

                printf("\nMensagem: ");
                fgets(msg, sizeof(msg), stdin);
                msg[strcspn(msg, "\n")] = 0;

                snprintf(buffer, sizeof(buffer), "ECHO %s", msg);
            }
            // GET_INFO
            else if (option == 2) {
                snprintf(buffer, sizeof(buffer), "GET_INFO");
            }

            // LIST_ALL username password
            else if (option == 4) {
                snprintf(buffer, sizeof(buffer),
                         "LIST_ALL %s %s",
                         current_user, current_pass);
            }

            // SEND sender password receiver message
            else if (option == 5) {
                char dest[50];
                char msg[900];

                printf("Enviar para: ");
                fgets(dest, sizeof(dest), stdin);
                dest[strcspn(dest, "\n")] = 0;

                printf("Mensagem: ");
                fgets(msg, sizeof(msg), stdin);
                msg[strcspn(msg, "\n")] = 0;

                snprintf(buffer, sizeof(buffer),
                         "SEND %s %s %s %s",
                         current_user, current_pass, dest, msg);
            }

            // CHECK_INBOX username password
            else if (option == 6) {
                snprintf(buffer, sizeof(buffer),
                         "CHECK_INBOX %s %s",
                         current_user, current_pass);
            }

            // APPROVE_USER admin password target_user
            else if (option == 7 && strcmp(current_user, "admin") == 0) {
                char user[50];

                printf("User a aprovar: ");
                fgets(user, sizeof(user), stdin);
                user[strcspn(user, "\n")] = 0;

                snprintf(buffer, sizeof(buffer),
                         "APPROVE_USER %s %s %s",
                         current_user, current_pass, user);
            }

            // DELETE_USER admin password target_user
            else if (option == 8 && strcmp(current_user, "admin") == 0) {
                char user[50];

                printf("User a apagar: ");
                fgets(user, sizeof(user), stdin);
                user[strcspn(user, "\n")] = 0;

                snprintf(buffer, sizeof(buffer),
                         "DELETE_USER %s %s %s",
                         current_user, current_pass, user);
            }

            else {
                printf(RED "Opção inválida!\n" RESET);
                close(sock);
                pause_screen();
                continue;
            }

            // Envia o comando montado para o servidor
            send(sock, buffer, strlen(buffer), 0);

            int n = recv(sock, buffer, BUF_SIZE - 1, 0);

            if (n > 0) {
                buffer[n] = '\0';

                // Mostra uma mensagem adequada consoante a resposta do servidor
                if (strstr(buffer, "AUTH_FAIL")) {
                    printf(RED "✖ Autenticação falhou. Faz login novamente.\n" RESET);
                }
                else if (strstr(buffer, "NOT_ADMIN")) {
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
                else if (strstr(buffer, "CANNOT_DELETE_ADMIN")) {
                    printf(RED "✖ Não é permitido apagar o admin!\n" RESET);
                }
                else if (strstr(buffer, "MSG_STORED")) {
                    printf(GREEN "✔ Mensagem guardada no servidor!\n" RESET);
                }
                else if (strstr(buffer, "INVALID_FORMAT")) {
                    printf(RED "✖ Formato de comando inválido!\n" RESET);
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
