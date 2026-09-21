#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <atomic_ops.h>

#define POOL_SIZE 32

void read_req(const int client, char request[512])
{
    int bytes_red = 0;

    while (bytes_red < 512)
    {
        int r = recv(client, request + bytes_red, 512 - bytes_red, 0);

        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }

        bytes_red += r;
    }
}

void send_res(const int client, char response[1024])
{
    int bytes_sent = 0;

    while (bytes_sent < 1024)
    {
        int r = send(client, response + bytes_sent, 1024 - bytes_sent, 0);

        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }

        bytes_sent += r;
    }
}

void serve(const unsigned long thrd, const int client) {

    printf("Atendendo cliente %d na thread %d\n", client, thrd);

    char request[512] = {0}, response[1024] = {0};

    int should_quit = 0;

    while (!should_quit)
    {

        read_req(client, request);

        printf("Cliente %d mandou: %s\n", client, request);

        if (strncmp(request, "QUIT", 4))
        {
            should_quit = 1;
        }
        
        sprintf(response,
                "Atendido por thread: %lu\n"
                "Mensagem: %s",
                thrd,
                request);

        send_res(client, response);

        memset(request, 0, 512);
        memset(response, 0, 1024);
    }

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

    init_pool();

    int socket_fd, client;

    const uint16_t port = (unsigned short)atoi(argv[1]);
    
    printf("%hu\n", port);

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

    printf("Server listening on %d\n", port);
    
    while (1)
    {
        client = accept(socket_fd, (struct sockaddr*) &address, &len);
        if (client == -1) continue;
        printf("Cliente %d conectado\n", client);
        
        pthread_mutex_lock(&pool.lock);
        while (pool.waiting_now >= 8) {
            printf("Cliente %d em espera\n", client);
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