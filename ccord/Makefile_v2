# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Nomes dos executáveis
SERVER = server
CLIENT = client

# Regra principal: compila tudo
all: $(SERVER) $(CLIENT)

# Compilar o servidor
$(SERVER): server.c
	$(CC) $(CFLAGS) server.c -o $(SERVER)

# Compilar o cliente
$(CLIENT): client.c
	$(CC) $(CFLAGS) client.c -o $(CLIENT)

# Limpar executáveis
clean:
	rm -f $(SERVER) $(CLIENT)

# Recompilar tudo do zero
rebuild: clean all
