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

char *port = "25501";
int port_num = 25501;
char *host = "::1";

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

int main()
{
    int sockfd, numbytes, clientfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char buffer[1024];

    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Rezultatas %s\n", gai_strerror(rv));
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

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Sistemos įvykio klaida");
        exit(1);
    }

    printf("Serveris: laukiama prisijungimų...\n");

    while(1)
    {
        sin_size = sizeof their_addr;
        clientfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (clientfd == -1)
        {
            perror("Nepavyko priimti kliento");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Serveris: gautas prisijungimas iš %s\n", s);

        if (!fork())
        {
            close(sockfd);
            
            if ((numbytes = recv(clientfd, buffer, 1023, 0)) == -1)
            {
                close(clientfd);
                perror("Nepavyko gauti žinutės");
                exit(1);
            }

            buffer[numbytes] = '\0';

            int i = 0;
            while (buffer[i])
            {
                buffer[i] = toupper(buffer[i]);
                i++;
            }

            if (send(clientfd, buffer, strlen(buffer), 0) == -1)
            {
                perror("Nepavyko išsiųsti žinutės");
            }

            close(clientfd);
            exit(0);
        }
        close(clientfd);
    }

    return 0;
}