#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

//char *port1 = "21661";
//char *host = "127.0.0.1";
//host = '::1'

void *get_in_addr(struct sockaddr *sa);

char* generate_fact();

int main(int argc, char **argv)
{
    char *host = "::1";
    int sockfd, numbytes;
    char *port;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char buffer[1024];

    port = argv[1];
    srand(time(0));

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Error %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        perror("Negalima atverti soketo");
        exit(1);
    }

    freeaddrinfo(servinfo);

    if ((numbytes = recv(sockfd, buffer, 1024, 0)) == -1)
    {
        close(sockfd);
        perror("Nepavyko gauti komandos: ATSIUSKVARDA");
        exit(1);
    }

    printf("%s", buffer);

    memset(buffer, 0, 1024);

    char *botname = "Bot\n ";

    if (send(sockfd, botname, strlen(botname), 0) == -1)
    {
        close(sockfd);
        perror("Nepavyko issiusti vardo");
        exit(1);
    }

    if ((numbytes = recv(sockfd, buffer, 1024, 0)) == -1)
    {
        close(sockfd);
        perror("Nepavyko gauti komandos: VARDASOK");
        exit(1);
    }

    if(strncmp(buffer, "VARDASOK", 8) != 0)
    {
        close(sockfd);
        perror("KLAIDA PRISKIRIANT VARDA");
        exit(1);
    }

    printf("%s", buffer);

    memset(buffer, 0, 1024);

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    while(1)
    {
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready == -1)
        {
            perror("Klaida klausant soketo");
            break;
        }
        else if (ready == 0)
        {
            memset(buffer, 0, 1024);
            strcpy(buffer, generate_fact());

            if (send(sockfd, buffer, strlen(buffer), 0) == -1)
            {
                perror("Nepavyko issiusti zinutes");
                break;
            }

            printf("BOTAS: %s", buffer);

            FD_ZERO(&read_fds);
            FD_SET(sockfd, &read_fds);
            
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
        }
        else
        {

            memset(buffer, 0, 1024);

            ssize_t res = recv(sockfd, buffer, sizeof(buffer), 0);
            if (res == -1)
            {
                perror("Klaida klausant soketo");
                break;
            }

            printf("Serveris ---> BOTAS: %s", buffer);

            FD_ZERO(&read_fds);
            FD_SET(sockfd, &read_fds);

            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
        }
    }

    close(sockfd);

    return 0;
}

char* generate_fact()
{
    char *facts[10] = 
    {
        "Aukščiausias pasaulio miškas yra Humboldt Redwoods Parke, Kalifornijoje (JAV). Medžių aukštis čia siekia 91 metrą.\n ",
        "Niujorke yra tiek restoranų, kad vienas  žmogus galėtų kas vakarą vakarieniauti vis kitame net 54 metus.\n ",
        "Aštuonkojis turi tris širdis, o jo kraujas yra šviesiai mėlynos spalvos.\n ",
        "Gyvatė gali miegoti 3 metus.\n ",
        "Auksas - toks retas metalas, kad per valandą pasaulyje yra randama daugiau geležies negu per visą istoriją buvo rasta aukso.\n ",
        "Kiekvieną dieną mūsų organizmas pagamina tiek energijos, kad jos pakaktų sunkvežimiu nuvažiuoti 30 km ar uždegti 10 vatų lemputę.\n ",
        "2,6 procento Microsoft Word vartotojų nežino, kaip Word redaktoriuje pakeisti šrifto stilių.\n ",
        "Lietuva užima vieną pirmųjų vietų pasaulyje pagal gyventojų aprūpinimą sparčiąja interneto prieiga.\n ",
        "Lietuva yra paskutinė Europoje šalis priėmusi krikščionybę.\n ",
        "Apie trečdalį visos Lietuvos teritorijos užima miškai ir parkai.\n "
    };

    int num = rand() % 9;

    return facts[num];
}