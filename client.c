#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define REQ_SIZE 512
#define RES_SIZE 1024

void clear() {

    char c;
    while ((c = getchar()) != '\n' && c != EOF);

}

bool sanitize(char *str) {

    size_t length = strlen(str);

    if (length > 0 && str[length - 1] == '\n')
    {
        str[length - 1] = 0;
        return false;
    }

    return true;
    
}

void get_request(char *req)
{
    fgets(req, REQ_SIZE, stdin);
    if (sanitize(req)) clear();
}

int send_request(const int sock, const char *req) {
    int bytes_enviados = 0;
    
    while (bytes_enviados < REQ_SIZE) {
        int r = send(sock, req + bytes_enviados, REQ_SIZE - bytes_enviados, 0);
        
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }
        
        bytes_enviados += r;
    }
    return bytes_enviados;
}

int recv_response(const int sock, char *req) {
    int bytes_lidos = 0;
    
    while (bytes_lidos < RES_SIZE) {
        int r = recv(sock, req + bytes_lidos, RES_SIZE - bytes_lidos, 0);
        
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }
        
        bytes_lidos += r;
    }
    return bytes_lidos;
}


int main(int argc, char **argv) {

    char request[REQ_SIZE] = {0}, response[RES_SIZE] = {0};
    
    int socket_fd;
    
    if (!(argv[1] && argv[2]))
    {
        puts("uso: ./client endereco porta");
        return 1;
    }

    const char *addr = argv[1];
    const uint16_t port = htons((uint16_t)atoi(argv[2]));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = inet_addr(addr),
        .sin_port = port
    };

    int len = sizeof(address);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        puts("Erro ao criar socket");
        goto error_ret;
    }

    if (connect(socket_fd, (struct sockaddr *) &address, len) < 0)
    {
        puts("Erro ao conectar");
        close(socket_fd);
        goto error_close;
    }
    
    int should_quit = 0;

    while (!should_quit) {
        
        get_request(request);

        if (send_request(socket_fd, request) < 0) {
            puts("Erro ao enviar mensagem");
            goto error_close;
        }

        if (recv_response(socket_fd, response) < 0) {
            puts("Erro ao receber mensagem");
            goto error_close;
        }

        const uint8_t control = response[0];
        if (control == 0)
        {
            should_quit = 1;
        }

        const char *message = response + 1;

        puts(message);

        memset(request, 0, REQ_SIZE);
        memset(response, 0, RES_SIZE);

    }

    close(socket_fd);

    return 0;

    error_close:
    close(socket_fd);

    error_ret:
    return -1;
}