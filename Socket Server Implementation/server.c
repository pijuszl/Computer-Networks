#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

struct client
{
    char *name;
    int clientfd;
    struct client *next;
};

struct client *header = NULL;
pthread_mutex_t clients_mutex;

void *get_in_addr(struct sockaddr *sa);
void  sigchld_handler(int s);
int   connect_server(char *port);
void *client_handler(void *arg);
void  add_user(struct client *user);
void  delete_user(int clientfd);
int   find_name(char *name);
void  send_message(char *buffer, char *sender);

int main(int argc, char **argv)
{
    int sockfd;
    char s[INET6_ADDRSTRLEN];
    struct sigaction sa;
    pthread_t tid;

    sockfd = connect_server(argv[1]);

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Sistemos įvykio klaida");
        exit(1);
    }

    printf("Serveris veikia... \n");

    int *clientfd;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    while(1)
    {
        sin_size = sizeof their_addr;
        clientfd = malloc(sizeof(int));
        *clientfd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (*clientfd == -1)
        {
            perror("Nepavyko priimti kliento");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Serveris: gautas prisijungimas iš %s\n", s);

        pthread_create(&tid,NULL,client_handler, clientfd);
    }

    return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int connect_server(char *port)
{
    char *host = "::1";
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int status;
    int yes = 1;

    int port_num = atoi(port); //25501;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Rezultatas %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            continue;
        }

        struct sockaddr_in6 server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(port_num);
        server_addr.sin6_addr = in6addr_any;

        if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        {
            close(sockfd);
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
        {
            continue;
        }

        if (listen(sockfd, 10) == -1)
        {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "Negalima atverti soketo\n");
        exit(1);
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

void *client_handler(void *arg)
{
    struct client *user;
    int clientfd;
    int byte_size;
    char buffer[1024];
    char username[1024];

    pthread_detach(pthread_self());

    clientfd = *((int *) arg);

    memset(buffer, 0, strlen(buffer));
    memset(username, 0, strlen(username));

    while(1)
    {
        char *msg = "ATSIUSKVARDA\n";
        if (send(clientfd, msg, strlen(msg), 0) == -1)
        {
                perror("Nepavyko išsiųsti žinutės: ATSIUSKVARDA\n");
                close(clientfd);
                return NULL;
        }

        if ((byte_size = recv(clientfd, username, 1024, 0)) == -1)
        {
                perror("Nepavyko gauti vardo\n");
                close(clientfd);
                return NULL;
        }

        username[byte_size-2] = '\0';
        //username[byte_size] = '\0';
        if(!find_name(username))
        {
            break;
        }
        memset(username, 0, 1024);
    }

    user = malloc(sizeof(struct client));
    if (user == NULL)
    {
        perror("Klaida priskiriant atmintį\n");
        close(clientfd);
        return NULL;
    }

    user->name = malloc(sizeof(username));
    memcpy(user->name, username, strlen(username)+1);
    user->clientfd = clientfd;

    pthread_mutex_lock(&clients_mutex);
    add_user(user);
    pthread_mutex_unlock(&clients_mutex);

    char *msg = "VARDASOK\n";
    if (send(user->clientfd, msg, strlen(msg), 0) == -1)
    {
        perror("Nepavyko išsiųsti žinutės: VARDASOK\n");
        close(clientfd);
        return NULL;
    }

    memset(buffer, 0, 1024);

    while((byte_size = recv(clientfd, buffer, 1024, 0)) != -1)
    {
        buffer[byte_size-2] = '\0';
        //buffer[byte_size] = '\0';
        printf("PRANESIMAS %s: %s\n", user->name, buffer);
        pthread_mutex_lock(&clients_mutex);
        send_message(buffer, user->name);
        pthread_mutex_unlock(&clients_mutex);
        memset(buffer, 0, 1024);
    }

    pthread_mutex_lock(&clients_mutex);
    delete_user(clientfd);
    pthread_mutex_unlock(&clients_mutex);
    close(clientfd);
    return NULL;
}

void add_user(struct client *user){

    if(header == NULL)
    {
        header = user;
        user->next = NULL;
    }
    else
    {
        user->next = header;

        header = user;
    }
}

void delete_user(int clientfd)
{
    struct client *user = header;
    struct client *previous = NULL;

    while(user->clientfd != clientfd)
    {
        previous = user;
        user = user->next;
    }

    if(previous == NULL)
    {
        header = user->next;
    }
    else
    {
        previous->next = user->next;
    }

    free(user);
}

int find_name(char *name)
{
    if(header == NULL)
    {
        return 0;
    }

    for (struct client *c = header; c != NULL; c = c->next)
    {
        int res = strcmp(c->name, name);
        if(res == 0)
        {
            return 1;
        }
    }

    return 0;
}

void send_message(char *buffer, char *sender)
{
    struct client *user = header;
    char msg[1024];

    memset(msg, 0, 1024);
    sprintf(msg, "PRANESIMAS %s: %s\n", sender, buffer);
    //printf(msg, "PRANESIMAS %s: %s\n", sender, buffer);

    while (user != NULL)
    {
        if (send(user->clientfd, msg, strlen(msg), 0) == -1)
        {
            continue;
        }

        user = user->next;
    }
}
