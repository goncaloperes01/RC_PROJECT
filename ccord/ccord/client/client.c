#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define BUF_SIZE 1024
#define MAX_FILE_SIZE 50000
#define UDP_PACKET_SIZE 65535

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    if (slash) return slash + 1;
    return path;
}

void clear_screen(void) {
    system("clear");
}

void pause_screen(void) {
    printf("\nPressiona ENTER para continuar...");
    getchar();
}

int connect_to_server(void) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket TCP");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Erro no IP do servidor");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao ligar ao servidor");
        close(sock);
        return -1;
    }

    return sock;
}

int recv_line_once(int sock, char *buffer, size_t size) {
    int n = recv(sock, buffer, size - 1, 0);
    if (n <= 0) return n;
    buffer[n] = '\0';
    return n;
}

int create_udp_socket(int *udp_port) {
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("Erro ao criar socket UDP");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0); // porta automática

    if (bind(udp_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Erro no bind UDP");
        close(udp_sock);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(udp_sock, (struct sockaddr*)&addr, &len) < 0) {
        perror("Erro ao obter porta UDP");
        close(udp_sock);
        return -1;
    }

    *udp_port = ntohs(addr.sin_port);
    return udp_sock;
}

int send_udp_file(int udp_sock, const char *my_username, const char *file_path, const char *peer_ip, int peer_port) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf(RED "[UDP] Não consegui abrir o ficheiro: %s\n" RESET, file_path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        printf(RED "[UDP] Erro ao ler tamanho do ficheiro\n" RESET);
        return 0;
    }

    if (file_size > MAX_FILE_SIZE) {
        fclose(file);
        printf(RED "[UDP] Ficheiro demasiado grande. Limite desta versão: %d bytes\n" RESET, MAX_FILE_SIZE);
        return 0;
    }

    const char *filename = base_name(file_path);
    char header[512];
    int header_len = snprintf(header, sizeof(header), "FILE %s %s %ld\n", my_username, filename, file_size);

    if (header_len <= 0 || header_len >= (int)sizeof(header)) {
        fclose(file);
        printf(RED "[UDP] Nome de ficheiro demasiado grande\n" RESET);
        return 0;
    }

    int packet_size = header_len + (int)file_size;
    char *packet = malloc(packet_size);
    if (!packet) {
        fclose(file);
        printf(RED "[UDP] Erro de memória\n" RESET);
        return 0;
    }

    memcpy(packet, header, header_len);

    if (file_size > 0) {
        size_t read_bytes = fread(packet + header_len, 1, file_size, file);
        if (read_bytes != (size_t)file_size) {
            free(packet);
            fclose(file);
            printf(RED "[UDP] Erro ao ler ficheiro\n" RESET);
            return 0;
        }
    }

    fclose(file);

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);

    if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
        free(packet);
        printf(RED "[UDP] IP inválido recebido do servidor\n" RESET);
        return 0;
    }

    int sent = sendto(udp_sock, packet, packet_size, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    free(packet);

    if (sent < 0) {
        perror("Erro no sendto UDP");
        return 0;
    }

    printf(GREEN "[UDP] Ficheiro enviado diretamente para %s:%d (%d bytes)\n" RESET, peer_ip, peer_port, sent);
    return 1;
}

void receive_udp_file(int udp_sock) {
    char packet[UDP_PACKET_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    int n = recvfrom(udp_sock, packet, sizeof(packet), 0, (struct sockaddr*)&sender_addr, &sender_len);
    if (n <= 0) return;

    char *newline = memchr(packet, '\n', n);
    if (!newline) {
        printf(YELLOW "[UDP] Pacote UDP desconhecido recebido\n" RESET);
        return;
    }

    int header_len = (int)(newline - packet) + 1;

    char header[512];
    int copy_len = header_len < (int)sizeof(header) - 1 ? header_len : (int)sizeof(header) - 1;
    memcpy(header, packet, copy_len);
    header[copy_len] = '\0';
    trim_newline(header);

    char tag[20], sender[50], filename[200];
    long expected_size;

    if (sscanf(header, "%19s %49s %199s %ld", tag, sender, filename, &expected_size) != 4 || strcmp(tag, "FILE") != 0) {
        printf(YELLOW "[UDP] Cabeçalho UDP inválido\n" RESET);
        return;
    }

    int content_size = n - header_len;
    if (content_size < 0) content_size = 0;

    char safe_filename[260];
    snprintf(safe_filename, sizeof(safe_filename), "received_%s", base_name(filename));

    FILE *out = fopen(safe_filename, "wb");
    if (!out) {
        printf(RED "[UDP] Não consegui criar ficheiro recebido\n" RESET);
        return;
    }

    if (content_size > 0) {
        fwrite(packet + header_len, 1, content_size, out);
    }
    fclose(out);

    char sender_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));

    printf(GREEN "\n[UDP] Ficheiro recebido de %s (%s:%d): %s (%d/%ld bytes)\n> " RESET,
           sender,
           sender_ip,
           ntohs(sender_addr.sin_port),
           safe_filename,
           content_size,
           expected_size);
    fflush(stdout);
}

void print_chat_help(void) {
    printf(CYAN "\nComandos Phase 3:\n" RESET);
    printf("  /join #general              entrar num canal (/join general também funciona)\n");
    printf("  /msg mensagem               enviar mensagem ao canal atual\n");
    printf("  texto normal                também envia ao canal atual\n");
    printf("  /who                        ver users no canal atual\n");
    printf("  /channels                   listar canais ativos\n");
    printf("  /list_all                   listar utilizadores aprovados\n");
    printf("  /check_inbox                ver mensagens antigas guardadas\n");
    printf("  /send user mensagem         guardar mensagem privada antiga\n");
    printf("  /sendfile user ficheiro     enviar ficheiro pequeno por UDP\n");
    printf("  /get_info                   ver info do servidor\n");
    printf("  /approve user               admin aprova user\n");
    printf("  /delete user                admin apaga user\n");
    printf("  /logout                     sair\n\n");
}

void handle_tcp_message(char *msg, int udp_sock, const char *my_username, char *pending_file_path, size_t pending_size) {
    char *saveptr;
    char *line = strtok_r(msg, "\n", &saveptr);

    while (line != NULL) {
        if (strncmp(line, "PEER ", 5) == 0) {
            char target_user[50], peer_ip[100], filename[200];
            int peer_port;

            if (sscanf(line, "PEER %49s %99s %d %199s", target_user, peer_ip, &peer_port, filename) == 4) {
                if (pending_file_path[0] == '\0') {
                    printf(YELLOW "[UDP] Servidor enviou peer, mas não há ficheiro pendente\n" RESET);
                } else {
                    printf(CYAN "[UDP] Peer encontrado: %s %s:%d\n" RESET, target_user, peer_ip, peer_port);
                    send_udp_file(udp_sock, my_username, pending_file_path, peer_ip, peer_port);
                    pending_file_path[0] = '\0';
                }
            } else {
                printf(RED "Resposta PEER inválida\n" RESET);
            }
        } else if (strcmp(line, "LOGOUT_OK") == 0) {
            printf(YELLOW "Servidor confirmou logout.\n" RESET);
        } else {
            printf("\n%s\n", line);
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    printf("> ");
    fflush(stdout);
    (void)pending_size;
}

void chat_loop(int tcp_sock, const char *username) {
    int udp_port = 0;
    int udp_sock = create_udp_socket(&udp_port);
    if (udp_sock < 0) {
        printf(RED "Não foi possível criar socket UDP. O chat TCP continua, mas /sendfile não vai funcionar.\n" RESET);
    } else {
        char udp_cmd[100];
        snprintf(udp_cmd, sizeof(udp_cmd), "/udp_port %d\n", udp_port);
        send(tcp_sock, udp_cmd, strlen(udp_cmd), 0);
        printf(GREEN "UDP ativo na porta %d\n" RESET, udp_port);
    }

    print_chat_help();
    printf(YELLOW "Dica: começa com /join #general\n" RESET);
    printf("> ");
    fflush(stdout);

    char pending_file_path[512] = "";

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(tcp_sock, &read_fds);

        int max_fd = tcp_sock;

        if (udp_sock >= 0) {
            FD_SET(udp_sock, &read_fds);
            if (udp_sock > max_fd) max_fd = udp_sock;
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("Erro no select do cliente");
            break;
        }

        if (FD_ISSET(tcp_sock, &read_fds)) {
            char buffer[BUF_SIZE * 2];
            int n = recv(tcp_sock, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0) {
                printf(RED "\nLigação ao servidor terminou.\n" RESET);
                break;
            }

            buffer[n] = '\0';
            handle_tcp_message(buffer, udp_sock, username, pending_file_path, sizeof(pending_file_path));
        }

        if (udp_sock >= 0 && FD_ISSET(udp_sock, &read_fds)) {
            receive_udp_file(udp_sock);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char line[BUF_SIZE];

            if (!fgets(line, sizeof(line), stdin)) {
                break;
            }

            trim_newline(line);

            if (line[0] == '\0') {
                printf("> ");
                fflush(stdout);
                continue;
            }

            if (strcmp(line, "/help") == 0) {
                print_chat_help();
                printf("> ");
                fflush(stdout);
                continue;
            }

            if (strncmp(line, "/sendfile ", 10) == 0) {
                char target_user[50], file_path[512];

                if (sscanf(line, "/sendfile %49s %511s", target_user, file_path) != 2) {
                    printf(RED "Uso: /sendfile user ficheiro\n" RESET);
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                if (udp_sock < 0) {
                    printf(RED "UDP não está ativo neste cliente.\n" RESET);
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                FILE *test = fopen(file_path, "rb");
                if (!test) {
                    printf(RED "Ficheiro não encontrado: %s\n" RESET, file_path);
                    printf("> ");
                    fflush(stdout);
                    continue;
                }
                fclose(test);

                snprintf(pending_file_path, sizeof(pending_file_path), "%s", file_path);

                char request[BUF_SIZE];
                snprintf(request, sizeof(request), "/sendfile %s %s\n", target_user, base_name(file_path));
                send(tcp_sock, request, strlen(request), 0);

                printf(YELLOW "A pedir IP/porta UDP de %s ao servidor...\n" RESET, target_user);
                printf("> ");
                fflush(stdout);
                continue;
            }

            if (strcmp(line, "/logout") == 0 || strcmp(line, "/exit") == 0) {
                char out[] = "/logout\n";
                send(tcp_sock, out, strlen(out), 0);
                break;
            }

            char out[BUF_SIZE + 5];
            snprintf(out, sizeof(out), "%s\n", line);
            send(tcp_sock, out, strlen(out), 0);

            printf("> ");
            fflush(stdout);
        }
    }

    if (udp_sock >= 0) close(udp_sock);
}

void do_register(void) {
    char username[50], password[50];
    char buffer[BUF_SIZE];

    printf("Username: ");
    scanf("%49s", username);
    printf("Password: ");
    scanf("%49s", password);
    getchar(); // limpar ENTER

    int sock = connect_to_server();
    if (sock < 0) return;

    // Ignora mensagem CONNECTED do servidor
    recv_line_once(sock, buffer, sizeof(buffer));

    char request[BUF_SIZE];
    snprintf(request, sizeof(request), "REGISTER %s %s\n", username, password);
    send(sock, request, strlen(request), 0);

    int n = recv_line_once(sock, buffer, sizeof(buffer));
    if (n > 0) printf("Resposta: %s", buffer);

    close(sock);
}

void do_login(void) {
    char username[50], password[50];
    char buffer[BUF_SIZE];

    printf("Username: ");
    scanf("%49s", username);
    printf("Password: ");
    scanf("%49s", password);
    getchar(); // limpar ENTER

    int sock = connect_to_server();
    if (sock < 0) return;

    // Ignora mensagem CONNECTED do servidor
    recv_line_once(sock, buffer, sizeof(buffer));

    char request[BUF_SIZE];
    snprintf(request, sizeof(request), "LOGIN %s %s\n", username, password);
    send(sock, request, strlen(request), 0);

    int n = recv_line_once(sock, buffer, sizeof(buffer));
    if (n <= 0) {
        printf(RED "Servidor não respondeu ao login.\n" RESET);
        close(sock);
        return;
    }

    trim_newline(buffer);

    if (strcmp(buffer, "LOGIN_OK") == 0) {
        printf(GREEN "Login OK. Entraste como %s.\n" RESET, username);
        chat_loop(sock, username);
    } else if (strcmp(buffer, "NOT_APPROVED") == 0) {
        printf(YELLOW "Conta ainda não aprovada pelo admin.\n" RESET);
    } else {
        printf(RED "Login falhou: %s\n" RESET, buffer);
    }

    close(sock);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    while (1) {
        int option;

        printf(CYAN "\n===== CLIENTE PHASE 3 =====\n" RESET);
        printf("1. Register\n");
        printf("2. Login / Chat em tempo real\n");
        printf("0. Sair\n");
        printf("Opção: ");

        if (scanf("%d", &option) != 1) {
            printf(RED "Opção inválida.\n" RESET);
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        getchar(); // limpar ENTER

        if (option == 1) {
            do_register();
        } else if (option == 2) {
            do_login();
        } else if (option == 0) {
            printf("A sair...\n");
            break;
        } else {
            printf(RED "Opção inválida.\n" RESET);
        }
    }

    return 0;
}
