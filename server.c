#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <atomic_ops.h>

#define POOL_SIZE 32

#define REQ_SIZE 512
#define RES_SIZE 1024

#define DEBUG(str, ...) do { \
    char timestr[16] = {0}; \
    const time_t t = time(NULL);\
    const struct tm *info = localtime(&t); \
    strftime(timestr, sizeof(timestr), "[%H:%M:%S] ", info);  \
    printf(timestr); \
    printf(str, __VA_ARGS__); \
} while (0)

int read_req(const int client, char *request)
{
    int bytes_red = 0;

    while (bytes_red < REQ_SIZE)
    {
        int r = recv(client, request + bytes_red, REQ_SIZE - bytes_red, 0);

        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }

        bytes_red += r;
    }

    return bytes_red;
}

int send_res(const int client, const char *response)
{
    int bytes_sent = 0;

    puts("aqui 2");

    while (bytes_sent < RES_SIZE)
    {
        int r = send(client, response + bytes_sent, RES_SIZE - bytes_sent, 0);

        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }

        bytes_sent += r;
    }

    return bytes_sent;
}

void serve(const unsigned long thrd, const int client) {

    DEBUG("Atendendo cliente %d na thread %d\n", client, thrd);

    char request[REQ_SIZE] = {0}, response[RES_SIZE] = {0};

    int should_quit = 0;

    while (!should_quit)
    {

        int received = read_req(client, request);

        if (received <= 0)
        {
            should_quit = 1;
            goto cleanup;
        }

        DEBUG("Cliente %d mandou: %s\n", client, request);

        if (strcmp(request, "quit") == 0)
        {
            should_quit = 1;
            response[0] = 0x0;
            sprintf(response + 1,
                    "quitting...");
        }
        else if (strncmp(request, "echo", 4) == 0) {
            if (request[4] == 0)
            {
                response[0] = 0x2;

                sprintf(response + 1,
                        "error: uso: echo (espaço) [mensagem (len >= 0)]");
            }
            else if (request[4] == ' ') {
                response[0] = 0x1;
                sprintf(response + 1,
                        "ECHO: %s",
                        request + 5);
            }
            else {
                goto unsupported;
            }
        }
        else {
        unsupported:
            response[0] = 0xff;
            sprintf(response + 1,
                    "NOT SUPPORTED");            
        }

        send_res(client, response);

    cleanup:
        memset(request, 0, REQ_SIZE);
        memset(response, 0, RES_SIZE);
    }

    DEBUG("Cliente %d desconectado\n", client);

    close(client);
}


struct thread_arg_t
{
    struct pool_t *pool;
    size_t thrd;
};


static struct pool_t
{
    pthread_t threads[POOL_SIZE];
    struct thread_arg_t args[POOL_SIZE];
    pthread_cond_t tem_trabalho;
    pthread_cond_t tem_espaco;
    pthread_mutex_t lock;
    int waiting_clients[8];
    int inicio;
    int waiting_now;    
} pool = {0};


void *thread_run(void *arg) {

    const struct thread_arg_t args = *(struct thread_arg_t*) arg;

    struct pool_t *pool = args.pool;
    const int this_thrd = args.thrd;

    while (1)
    {
        pthread_mutex_lock(&pool->lock);

        while (pool->waiting_now == 0)
        {
            pthread_cond_wait(&pool->tem_trabalho, &pool->lock);
        }

        const int client = pool->waiting_clients[pool->inicio];
        pool->inicio = (pool->inicio + 1) % 8;
        pool->waiting_now--;

        pthread_cond_signal(&pool->tem_espaco);
        pthread_mutex_unlock(&pool->lock);

        serve(this_thrd, client);
    }
}

void init_pool() {

    pthread_mutex_init(&pool.lock, NULL);
    pthread_cond_init(&pool.tem_trabalho, NULL);
    pthread_cond_init(&pool.tem_espaco, NULL);

    for (size_t i = 0; i < POOL_SIZE; i++)
    {
        pool.args[i].pool = &pool;
        pool.args[i].thrd = i;

        pthread_create(&pool.threads[i], NULL, thread_run, &pool.args[i]);
    }


}

int main(int argc, char **argv) {

    if (!argv[1])
    {
        puts("uso: ./server porta");
        return 1;
    }
    
    init_pool();

    int socket_fd, client;

    const uint16_t port = htons((uint16_t)atoi(argv[1]));
    
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = INADDR_ANY,
        .sin_port = port
    };

    int len = sizeof(address);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        puts("Erro ao criar socket");
        return -1;
    }

    if (bind(socket_fd, (const struct sockaddr*) &address, len) < 0) {
        puts("Erro ao ligar socket à porta");
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, SOMAXCONN) < 0) {
        puts("Erro ao abrir conexões");
        close(socket_fd);
        return -1;
    }

    DEBUG("Server listening on %d\n", ntohs(port));
    
    while (1)
    {
        client = accept(socket_fd, (struct sockaddr*) &address, &len);
        if (client == -1) continue;
        DEBUG("Cliente %d conectado\n", client);
        
        pthread_mutex_lock(&pool.lock);
        while (pool.waiting_now >= 8) {
            DEBUG("Cliente %d em espera\n", client);
            pthread_cond_wait(&pool.tem_espaco, &pool.lock);
        }
        pool.waiting_clients[(pool.inicio + pool.waiting_now) % 8] = client;
        pool.waiting_now++;
        
        pthread_cond_signal(&pool.tem_trabalho);
        pthread_mutex_unlock(&pool.lock);

    }

    close(socket_fd);
    return 0;

}