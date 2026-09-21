#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 8080

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
    fgets(req, 512, stdin);
    if (sanitize(req)) clear();
}

int send_request(const int sock, const char *req) {
    int bytes_enviados = 0;
    
    while (bytes_enviados < 512) {
        int r = send(sock, req + bytes_enviados, 512 - bytes_enviados, 0);
        
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
    
    while (bytes_lidos < 1024) {
        int r = recv(sock, req + bytes_lidos, 1024 - bytes_lidos, 0);
        
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

    char request[512] = {0}, response[1024] = {0};
    
    int socket_fd;
    
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = INADDR_ANY,
        .sin_port = 8080
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
    
    do {
        
        get_request(request);

        if (send_request(socket_fd, request) < 0) {
            puts("Erro ao enviar mensagem");
            goto error_close;
        }

        printf("Enviado %s\n", request);

        if (recv_response(socket_fd, response) < 0) {
            puts("Erro ao receber mensagem");
            goto error_close;
        }

        printf("Recebido %s\n", response);

        memset(request, 0, 512);
        memset(response, 0, 1024);

    } while (strcmp(response, "QUIT FROM SERVER"));

    close(socket_fd);

    return 0;

    error_close:
    close(socket_fd);

    error_ret:
    return -1;
}