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
        // apresentado quando o utilizador ainda não está autenticado
        if (!logged_in) {
            clear_screen();

            int option;
            char username[50], password[50];

            // interface textual
            printf(CYAN "\n=========================\n");
            printf("        C-CORD v1.0\n");
            printf("=========================\n" RESET);
            printf(" [1] Register\n");
            printf(" [2] Login\n");
            printf(" [3] Exit\n");
            printf("=========================\n");
            printf("Escolha: ");
            scanf("%d", &option);
            getchar(); // limpa o buffer do stdin (remove '\n')

            // opção de saída do programa
            if (option == 3) {
                printf(YELLOW "A sair...\n" RESET);
                break;
            }

            // leitura de credenciais do utilizador
            printf("\nUsername: ");
            fgets(username, 50, stdin);
            username[strcspn(username, "\n")] = 0; // remove o '\n' do fgets

            printf("Password: ");
            fgets(password, 50, stdin);
            password[strcspn(password, "\n")] = 0;

            printf("\nA processar...\n");
            sleep(1);

            // Criação de um socket TCP
            // necessário para estabelecer comunicação com o servidor
            sock = socket(AF_INET, SOCK_STREAM, 0);

            // Configuração do endereço do servidor
            server_addr.sin_family = AF_INET; // IPv4
            server_addr.sin_port = htons(PORT); // conversão da porta para formato de rede
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); 
            // converte o IP em formato texto para binário

            // Estabelece ligação com o servidor
            // se falhar, a comunicação não será possível
            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

            // Construção do comando a enviar ao servidor
            // protocolo simples baseado em texto
            if (option == 1) {
                sprintf(buffer, "REGISTER %s %s", username, password);
            } else if (option == 2) {
                sprintf(buffer, "LOGIN %s %s", username, password);
            } else {
                printf(RED "Opção inválida!\n" RESET);
                pause_screen();
                continue;
            }

            // Envio do comando ao servidor
            send(sock, buffer, strlen(buffer), 0);

            // Receção da resposta do servidor
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0'; // garante terminação correta da string

                // Interpretação da resposta com base em palavras-chave
                if (strstr(buffer, "REGISTERED")) {
                    printf(GREEN "✔ Registo feito com sucesso!\n" RESET);
                }
                else if (strstr(buffer, "USER_EXISTS")) {
                    printf(RED "✖ Utilizador já existe!\n" RESET);
                }
                else if (strstr(buffer, "LOGIN_OK")) {
                    printf(GREEN "✔ Login bem sucedido!\n" RESET);
                    logged_in = 1; // muda estado para autenticado
                    strcpy(current_user, username); // guarda utilizador atual
                }
                else if (strstr(buffer, "LOGIN_FAIL")) {
                    printf(RED "✖ Credenciais erradas!\n" RESET);
                }
                else {
                    printf("Servidor: %s\n", buffer);
                }
            }

            // Fecha o socket após a operação
            // cada pedido usa uma nova ligação (modelo request-response)
            close(sock);
            pause_screen();
        }

        // ---------- MENU APÓS LOGIN ----------
        // apresentado após autenticação bem sucedida
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

            // logout apenas altera o estado local (não comunica com o servidor)
            if (option == 3) {
                printf(YELLOW "Logout feito.\n" RESET);
                logged_in = 0;
                pause_screen();
                continue;
            }

            // Cria novo socket para nova comunicação
            sock = socket(AF_INET, SOCK_STREAM, 0);

            // Configuração do servidor novamente
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            // Estabelece ligação
            connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

            // Construção do comando
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

            // envio do pedido
            send(sock, buffer, strlen(buffer), 0);

            // receção da resposta
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf(GREEN "\nResposta do servidor:\n%s\n" RESET, buffer);
            }

            // fecha ligação após resposta
            close(sock);
            pause_screen();
        }
    }

    return 0;
}
