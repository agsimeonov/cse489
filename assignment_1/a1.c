#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include <termios.h>
#include <sys/stat.h>

#define HELP 7173139
#define EXIT 8210640
#define MYIP 7162136
#define MYPORT 493467959
#define CONNECT 275095132
#define LIST 8250647
#define TERMINATE 346277688
#define UPLOAD 208238319
#define DOWNLOAD 392632745
#define CREATOR 523308712
#define PACKET_SIZE 512

struct connection { //struct holding everying needed by each connection
    int sockfd; //the file descriptor
    char *ip; //remote ip
    char *hostname; //remote hostname
    char *fname; //the filename of the file downloaded
    time_t ftime; //the time at which file download/upload began
    suseconds_t futime; //the time at which file download/upload began
    off_t fsize; //file size
    struct connection *next; //linked list part
    struct connection *prev; //linked list part
};

int dir_num = 0;
int input_flag = 0;

int main(int argc, char *argv[]) {
    int port; //port in argv
    char hostname[_POSIX_HOST_NAME_MAX]; //local hostname
    int getaddrinfo_value; 
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    struct addrinfo *res_iterator;
    int sockfd; //accept socket
    int enable_so_reuseaddr = 1;
    struct sockaddr_storage accept_addr;
    socklen_t addr_size = sizeof(accept_addr);
    int new_sockfd; //socket for new connections
    int nfds;
    fd_set fdset_main;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("8.8.8.8"); //for local ip check
    address.sin_port = htons(53); //for local ip check
    socklen_t address_len = sizeof(address);
    char myip[INET_ADDRSTRLEN];
    int cnct_num = 0;
    struct connection *cnct = NULL; 
    
    /* Check the local IP using Google DNS */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket() error");
        exit(1);
    }
    if((connect(sockfd, (const struct sockaddr *) &address, sizeof(address))) == -1) {
        perror("connect() error");
        exit(1);
    }
    memset(&address, 0, sizeof(address));
    getsockname(sockfd, (struct sockaddr *) &address, &address_len);
    inet_ntop(AF_INET, &address.sin_addr, myip, INET_ADDRSTRLEN);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    
    //Get port from argv
    if(argc == 2) {
        port = atoi(argv[1]);
        if((port < 1) || (port > 65535)) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            exit(1);
        }
    }
    else {
        fprintf(stderr, "Invalid command line parameters\n");
        exit(1);
    }
    
    //Get the local host name
    gethostname(hostname, sizeof(hostname));
    
    //Populate res for establishing the socket
    if((getaddrinfo_value = getaddrinfo(hostname, argv[1], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(getaddrinfo_value));
        exit(1);
    }
    
    //Establish socket
    for(res_iterator = res; res_iterator != NULL; res_iterator = res_iterator->ai_next) {
        if((sockfd = socket(res_iterator->ai_family, res_iterator->ai_socktype, res_iterator->ai_protocol)) == -1) {
            perror("socket() error");
            continue;
        }
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable_so_reuseaddr, sizeof(int)) == -1) {
            perror("setsockopt() error");
            exit(1);
        }
        if(bind(sockfd, res_iterator->ai_addr, res_iterator->ai_addrlen) == -1) {
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            perror("bind() error");
            continue;
        }
        break;
    }
    
    if(res_iterator == NULL) {
        fprintf(stderr, "bind() error: Could not bind\n");
        exit(1);
    }
    else {
        freeaddrinfo(res);
    }
    
    //Listen
    if(listen(sockfd, SOMAXCONN) == -1) {
        perror("listen() error");
        exit(1);
    }
    
    FD_ZERO(&fdset_main);
    FD_SET(sockfd, &fdset_main);
    FD_SET(STDIN_FILENO, &fdset_main);
    nfds = sockfd > STDIN_FILENO ? sockfd + 1 : STDIN_FILENO + 1;
    
    while(1) {
        fd_set fdset = fdset_main;
        int i;
        
        //Select
        if(select(nfds+1, &fdset, NULL, NULL, NULL) == -1) {
            perror("select() error");
            exit(1);
        }
        
        for(i = 0; i <= nfds; i++) {
            if(FD_ISSET(i, &fdset)) {
                if(i == sockfd) { //Accepts new connections
                    if((new_sockfd = accept(sockfd, (struct sockaddr*)&accept_addr, &addr_size)) == -1) {
                        perror("accept() error");
                        continue;
                    }
                    char *remoteip = (char *)calloc(1, INET_ADDRSTRLEN); //stores remote ip
                    sprintf(remoteip, "%s", inet_ntoa(((struct sockaddr_in*)&accept_addr)->sin_addr)); //get remote ip
                    char *remotename = (char *)calloc(1, _POSIX_HOST_NAME_MAX); //stores remote hostname
                    getnameinfo((struct sockaddr*)&accept_addr, sizeof(*((struct sockaddr*)&accept_addr)), remotename, _POSIX_HOST_NAME_MAX, NULL, 0, NI_NOFQDN); //get remote hostname
                    //Where self connections are not allowed
                    if((strncmp(myip, remoteip, INET_ADDRSTRLEN) == 0) || 
                       (strncmp(hostname, remotename, _POSIX_HOST_NAME_MAX) == 0) || 
                       (strncmp(remoteip, "127.0.0", 7) == 0)) {
                        send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                        shutdown(new_sockfd, SHUT_RDWR);
                        close(new_sockfd);
                        free(remoteip);
                        free(remotename);
                        puts("Self connections not allowed!");
                    }
                    else { //Allows connections
                        struct connection *new_cnct;
                        switch(cnct_num) {
                            case 0:
                                send(new_sockfd, "+ALLOW_CONNECTION\n", 18, 0);
                                new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                new_cnct->sockfd = new_sockfd;
                                new_cnct->ip = remoteip;
                                new_cnct->hostname = remotename;
                                cnct = new_cnct;
                                cnct_num = 1;
                                FD_SET(new_sockfd, &fdset_main);
                                nfds = new_sockfd > nfds ? new_sockfd + 1 : nfds;
                                printf("1: %s has connected!\n", remotename);
                                break;
                            case 1:
                                if(strncmp(cnct->ip, remoteip, INET_ADDRSTRLEN) == 0) {
                                    send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                                    shutdown(new_sockfd, SHUT_RDWR);
                                    close(new_sockfd);
                                    free(remoteip);
                                    free(remotename);
                                    puts("Duplicate connections not allowed!");
                                }
                                else {
                                    send(new_sockfd, "+ALLOW_CONNECTION\n", 18, 0);
                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                    new_cnct->sockfd = new_sockfd;
                                    new_cnct->ip = remoteip;
                                    new_cnct->hostname = remotename;
                                    new_cnct->prev = cnct;
                                    cnct->next = new_cnct;
                                    cnct_num = 2;
                                    FD_SET(new_sockfd, &fdset_main);
                                    nfds = new_sockfd > nfds ? new_sockfd + 1 : nfds;
                                    printf("2: %s has connected!\n", remotename);
                                }
                                break;
                            case 2:
                                if((strncmp(cnct->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->ip, remoteip, INET_ADDRSTRLEN) == 0)) {
                                    send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                                    shutdown(new_sockfd, SHUT_RDWR);
                                    close(new_sockfd);
                                    free(remoteip);
                                    free(remotename);
                                    puts("Duplicate connections not allowed!");
                                }
                                else {
                                    send(new_sockfd, "+ALLOW_CONNECTION\n", 18, 0);
                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                    new_cnct->sockfd = new_sockfd;
                                    new_cnct->ip = remoteip;
                                    new_cnct->hostname = remotename;
                                    new_cnct->prev = cnct->next;
                                    cnct->next->next = new_cnct;
                                    cnct_num = 3;
                                    FD_SET(new_sockfd, &fdset_main);
                                    nfds = new_sockfd > nfds ? new_sockfd + 1 : nfds;
                                    printf("3: %s has connected!\n", remotename);
                                }
                                break;
                            case 3:
                                if((strncmp(cnct->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->next->ip, remoteip, INET_ADDRSTRLEN) == 0)) {
                                    send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                                    shutdown(new_sockfd, SHUT_RDWR);
                                    close(new_sockfd);
                                    free(remoteip);
                                    free(remotename);
                                    puts("Duplicate connections not allowed!");
                                }
                                else {
                                    send(new_sockfd, "+ALLOW_CONNECTION\n", 18, 0);
                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                    new_cnct->sockfd = new_sockfd;
                                    new_cnct->ip = remoteip;
                                    new_cnct->hostname = remotename;
                                    new_cnct->prev = cnct->next->next;
                                    cnct->next->next->next = new_cnct;
                                    cnct_num = 4;
                                    FD_SET(new_sockfd, &fdset_main);
                                    nfds = new_sockfd > nfds ? new_sockfd + 1 : nfds;
                                    printf("4: %s has connected!\n", remotename);
                                }
                                break;
                            case 4:
                                if((strncmp(cnct->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->next->ip, remoteip, INET_ADDRSTRLEN) == 0) ||
                                   (strncmp(cnct->next->next->next->ip, remoteip, INET_ADDRSTRLEN) == 0)) {
                                    send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                                    shutdown(new_sockfd, SHUT_RDWR);
                                    close(new_sockfd);
                                    free(remoteip);
                                    free(remotename);
                                    puts("Duplicate connections not allowed!");
                                }
                                else {
                                    send(new_sockfd, "+ALLOW_CONNECTION\n", 18, 0);
                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                    new_cnct->sockfd = new_sockfd;
                                    new_cnct->ip = remoteip;
                                    new_cnct->hostname = remotename;
                                    new_cnct->prev = cnct->next->next->next;
                                    cnct->next->next->next->next = new_cnct;
                                    cnct_num = 5;
                                    FD_SET(new_sockfd, &fdset_main);
                                    nfds = new_sockfd > nfds ? new_sockfd + 1 : nfds;
                                    printf("5: %s has connected!\n", remotename);
                                }
                                break;
                            default:
                                send(new_sockfd, "+DENY_CONNECTION\n", 17, 0);
                                shutdown(new_sockfd, SHUT_RDWR);
                                close(new_sockfd);
                                free(remoteip);
                                free(remotename);
                                puts("More than 5 connections not allowed!");
                        }
                    }
                }
                else if(i == STDIN_FILENO) { //Command Line Parameters by Labels
                    char option[10] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
                    long option_switch;
                    char c = getchar();
                    
                    for(i = 0; (c != '\n') && (c != ' '); i++) {
                        if(i == 9) {
                            goto next_option;
                        }
                        else {
                            option[i] = c;
                            c = getchar();
                        }
                    }
                    
                    char half_option[6] = {option[0], option[1], option[2], option[3], option[4], '\0'};
                    option_switch = (i < 5) ? a64l(option) : a64l(&half_option[0]) + a64l(&option[5]); //Encodes the Option names
                    
                    switch(option_switch) {
                        case HELP:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                            puts("Avaliable options:\nHELP\t\t\t- Lists available options\nEXIT\t\t\t- Exits the process\nMYIP\t\t\t- Displays the IP address of this process\nMYPORT\t\t\t- Displays the port of this process\nCONNECT hostname port\t- Connects to hostname @ port\nLIST\t\t\t- Displays current connections and their # (1-5)\nTERMINATE #\t\t- Terminates connection associated with # (1-5)\nUPLOAD # file\t\t- Uploads file to # (1-5)\nDOWNLOAD # file...\t- Downloads a file from each # (1-5)\nCREATOR\t\t\t- Displays creator name and email\n");
                            break;
                        case EXIT:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                            switch(cnct_num) {
                                case 5:
                                    send(cnct->next->next->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                    shutdown(cnct->next->next->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->next->next->sockfd);
                                case 4:
                                    send(cnct->next->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                    shutdown(cnct->next->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->next->sockfd);
                                case 3:
                                    send(cnct->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                    shutdown(cnct->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->sockfd);
                                case 2:
                                    send(cnct->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                    shutdown(cnct->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->sockfd);
                                case 1:
                                    send(cnct->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                    shutdown(cnct->sockfd, SHUT_RDWR);
                                    close(cnct->sockfd);
                                default:
                                    shutdown(sockfd, SHUT_RDWR);
                                    close(sockfd);
                            }
                            exit(0);
                        case MYIP:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                            printf("%s\n\n", myip);
                            break;
                        case MYPORT:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                            printf("Listening for incoming connections on port: %i\n\n", port);
                            break;
                        case CONNECT:
                            if(c == ' ') {
                                char givenhost[_POSIX_HOST_NAME_MAX];
                                char givenport[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
                                memset(givenhost, '\0', _POSIX_HOST_NAME_MAX);
                                memset(&hints, 0, sizeof(hints));
                                hints.ai_family = AF_UNSPEC;
                                hints.ai_socktype = SOCK_STREAM;
                                hints.ai_flags = AI_CANONNAME;
                                int connectfd;
                                char connect_status[19];
                                memset(connect_status, '\0', 19);
                                
                                while(c == ' ') {
                                    c = getchar();
                                }
                                for(i = 0; (c != '\n') && (c != ' ') && (i < _POSIX_HOST_NAME_MAX); i++) {
                                    givenhost[i] = c;
                                    c = getchar();
                                }
                                if(c != ' ') {
                                    goto next_option;
                                }
                                while(c == ' ') {
                                    c = getchar();
                                }
                                for(i = 0; (c != '\n') && (c != ' ') && (i < 6); i++) {
                                    givenport[i] = c;
                                    c = getchar();
                                }
                                if(c == ' ') {
                                    while(c == ' ') {
                                        c = getchar();
                                    }
                                }
                                if(c != '\n') {
                                    goto next_option;
                                }
                                
                                if(cnct_num <= 5) {
                                    if(getaddrinfo(givenhost, givenport, &hints, &res) != 0) {
                                        puts("Unable to establish connection!\n");
                                        break;
                                    }
                                    
                                    if((connectfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
                                        perror("socket() error");
                                        puts("Unable to establish connection!\n");
                                        break;
                                    }
                                    
                                    if(connect(connectfd, res->ai_addr, res->ai_addrlen) != 0) {
                                        perror("connect() error");
                                        close(connectfd);
                                        puts("Unable to establish connection!\n");
                                        break;
                                    }
                                    
                                    struct pollfd connect_poll[2];
                                    connect_poll[0].fd = connectfd;
                                    connect_poll[0].events = POLLIN;
                                    connect_poll[0].revents = POLLERR;

                                    puts("Attempting to establish connection. User interface temporarily unavailable!");
                                    FD_CLR(STDIN_FILENO, &fdset_main);
                                    i = poll(connect_poll, 1, 5000);
                                    if(i == 0) {
                                        shutdown(connectfd, SHUT_RDWR);
                                        close(connectfd);
                                        puts("Connection timed out!");
                                    }
                                    else {
                                        recv(connectfd, connect_status, 18, 0);
                                        if(strncmp(connect_status, "+ALLOW_CONNECTION\n", 18) == 0) {
                                            char *remoteip = (char *)calloc(1, INET_ADDRSTRLEN);
                                            inet_ntop(AF_INET, (res->ai_addr->sa_data + 2), remoteip, INET_ADDRSTRLEN);
                                            char *remotename = (char *)calloc(1, _POSIX_HOST_NAME_MAX);
                                            sprintf(remotename, res->ai_canonname, _POSIX_HOST_NAME_MAX);
                                            struct connection *new_cnct;
                                            switch(cnct_num) {
                                                case 0:
                                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                                    new_cnct->sockfd = connectfd;
                                                    new_cnct->ip = remoteip;
                                                    new_cnct->hostname = remotename;
                                                    cnct = new_cnct;
                                                    cnct_num = 1;
                                                    FD_SET(connectfd, &fdset_main);
                                                    nfds = connectfd > nfds ? connectfd + 1 : nfds;
                                                    printf("1: %s is now connected!\n", remotename);
                                                    break;
                                                case 1:
                                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                                    new_cnct->sockfd = connectfd;
                                                    new_cnct->ip = remoteip;
                                                    new_cnct->hostname = remotename;
                                                    new_cnct->prev = cnct;
                                                    cnct->next = new_cnct;
                                                    cnct_num = 2;
                                                    FD_SET(connectfd, &fdset_main);
                                                    nfds = connectfd > nfds ? connectfd + 1 : nfds;
                                                    printf("2: %s is now connected!\n", remotename);
                                                    break;
                                                case 2:
                                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                                    new_cnct->sockfd = connectfd;
                                                    new_cnct->ip = remoteip;
                                                    new_cnct->hostname = remotename;
                                                    new_cnct->prev = cnct->next;
                                                    cnct->next->next = new_cnct;
                                                    cnct_num = 3;
                                                    FD_SET(connectfd, &fdset_main);
                                                    nfds = connectfd > nfds ? connectfd + 1 : nfds;
                                                    printf("3: %s is now connected!\n", remotename);
                                                    break;
                                                case 3:
                                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                                    new_cnct->sockfd = connectfd;
                                                    new_cnct->ip = remoteip;
                                                    new_cnct->hostname = remotename;
                                                    new_cnct->prev = cnct->next->next;
                                                    cnct->next->next->next = new_cnct;
                                                    cnct_num = 4;
                                                    FD_SET(connectfd, &fdset_main);
                                                    nfds = connectfd > nfds ? connectfd + 1 : nfds;
                                                    printf("4: %s is now connected!\n", remotename);
                                                    break;
                                                case 4:
                                                    new_cnct = (struct connection *)malloc(sizeof(struct connection));
                                                    new_cnct->sockfd = connectfd;
                                                    new_cnct->ip = remoteip;
                                                    new_cnct->hostname = remotename;
                                                    new_cnct->prev = cnct->next->next->next;
                                                    cnct->next->next->next->next = new_cnct;
                                                    cnct_num = 5;
                                                    FD_SET(connectfd, &fdset_main);
                                                    nfds = connectfd > nfds ? connectfd + 1 : nfds;
                                                    printf("5: %s has connected!\n", remotename);
                                                    break;
                                            }
                                        }
                                        else if(strncmp(connect_status, "+DENY_CONNECTION\n", 17) == 0) {
                                            shutdown(connectfd, SHUT_RDWR);
                                            close(connectfd);
                                            puts("Connection refused!");
                                        }
                                        else {
                                             shutdown(connectfd, SHUT_RDWR);
                                             close(connectfd);
                                             puts("Connection could not be established!");
                                        }
                                    }
                                    tcflush(STDIN_FILENO, TCIFLUSH);
                                    FD_SET(STDIN_FILENO, &fdset_main);
                                    puts("User interface is once again available!\n");
                                }
                                else {
                                    puts("More than 5 connections not allowed!");
                                }
                                
                                freeaddrinfo(res);
                            }
                            else {
                                goto next_option;
                            }
                            break;
                        case LIST:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                        connection_list:
                            switch(cnct_num) {
                                case 5:
                                    printf("5: %s\n", cnct->next->next->next->next->hostname);
                                case 4:
                                    printf("4: %s\n", cnct->next->next->next->hostname);
                                case 3:
                                    printf("3: %s\n", cnct->next->next->hostname);
                                case 2:
                                    printf("2: %s\n", cnct->next->hostname);
                                case 1:
                                    printf("1: %s\n", cnct->hostname);
                            }
                            puts("");
                            break;
                        case TERMINATE:
                            if(c == ' ') {
                                while(c == ' ') {
                                    c = getchar();
                                }
                                char c_num[2] = {c, '\0'};
                                i = c != '\n' ? atoi(c_num) : -1;
                                if(i == -1) {
                                    goto next_option;
                                }
                                else {
                                    while(c != '\n') {
                                        c = getchar();
                                        if((c != ' ') && ( c != '\n')) {
                                            goto next_option;
                                        }
                                    }
                                }
                                switch(i) {
                                    case 1:
                                        if(cnct_num >= i) {
                                            send(cnct->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                            FD_CLR(cnct->sockfd, &fdset_main);
                                            shutdown(cnct->sockfd, SHUT_RDWR);
                                            close(cnct->sockfd);
                                            free(cnct->ip);
                                            free(cnct->hostname);
                                            if(cnct_num == 1) {
                                                free(cnct);
                                                cnct = NULL;
                                            }
                                            else {
                                                cnct = cnct->next;
                                                free(cnct->prev);
                                                cnct->prev = NULL;
                                            }
                                            cnct_num = cnct_num - 1;
                                            puts("The connection list is now:");
                                            goto connection_list;
                                        }
                                    case 2:
                                        if(cnct_num >= i) {
                                            send(cnct->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                            FD_CLR(cnct->next->sockfd, &fdset_main);
                                            shutdown(cnct->next->sockfd, SHUT_RDWR);
                                            close(cnct->next->sockfd);
                                            free(cnct->next->ip);
                                            free(cnct->next->hostname);
                                            if(cnct_num == 2) {
                                                free(cnct->next);
                                                cnct->next = NULL;
                                            }
                                            else {
                                                cnct->next = cnct->next->next;
                                                free(cnct->next->prev);
                                                cnct->next->prev = cnct;
                                            }
                                            cnct_num = cnct_num - 1;
                                            puts("The connection list is now:");
                                            goto connection_list;
                                        }
                                    case 3:
                                        if(cnct_num >= i) {
                                            send(cnct->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                            FD_CLR(cnct->next->next->sockfd, &fdset_main);
                                            shutdown(cnct->next->next->sockfd, SHUT_RDWR);
                                            close(cnct->next->next->sockfd);
                                            free(cnct->next->next->ip);
                                            free(cnct->next->next->hostname);
                                            if(cnct_num == 3) {
                                                free(cnct->next->next);
                                                cnct->next->next = NULL;
                                            }
                                            else {
                                                cnct->next->next = cnct->next->next->next;
                                                free(cnct->next->next->prev);
                                                cnct->next->next->prev = cnct->next;
                                            }
                                            cnct_num = cnct_num - 1;
                                            puts("The connection list is now:");
                                            goto connection_list;
                                        }
                                    case 4:
                                        if(cnct_num >= i) {
                                            send(cnct->next->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                            FD_CLR(cnct->next->next->next->sockfd, &fdset_main);
                                            shutdown(cnct->next->next->next->sockfd, SHUT_RDWR);
                                            close(cnct->next->next->next->sockfd);
                                            free(cnct->next->next->next->ip);
                                            free(cnct->next->next->next->hostname);
                                            if(cnct_num == 4) {
                                                free(cnct->next->next->next);
                                                cnct->next->next->next = NULL;
                                            }
                                            else {
                                                cnct->next->next->next = cnct->next->next->next->next;
                                                free(cnct->next->next->next->prev);
                                                cnct->next->next->next->prev = cnct->next->next;
                                            }
                                            cnct_num = cnct_num - 1;
                                            puts("The connection list is now:");
                                            goto connection_list;
                                        }
                                    case 5:
                                        if(cnct_num >= i) {
                                            send(cnct->next->next->next->next->sockfd, "-TERMINATE_CONNECTION\n", 22, 0);
                                            FD_CLR(cnct->next->next->next->next->sockfd, &fdset_main);
                                            shutdown(cnct->next->next->next->next->sockfd, SHUT_RDWR);
                                            close(cnct->next->next->next->next->sockfd);
                                            free(cnct->next->next->next->next->ip);
                                            free(cnct->next->next->next->next->hostname);
                                            free(cnct->next->next->next->next);
                                            cnct->next->next->next->next = NULL;
                                            cnct_num = cnct_num - 1;
                                            puts("The connection list is now:");
                                            goto connection_list;
                                        }
                                    default:
                                        printf("%i is not a valid connection!\n\n", i);
                                }
                            }
                            else {
                                goto next_option;
                            }
                            break;
                        case UPLOAD:
                            if(c == ' ') {
                                int num;
                                char filename[PATH_MAX];
                                memset(filename, 0, PATH_MAX);
                                
                                while(c == ' ') {
                                    c = getchar();
                                }
                                while(c == ' ') {
                                    c = getchar();
                                }
                                char c_num[2] = {c, '\0'};
                                num = c != '\n' ? atoi(c_num) : -1;
                                if(num == -1) {
                                    goto next_option;
                                }
                                c = getchar();
                                if(c != ' ') {
                                    goto next_option;
                                }
                                while(c == ' ') {
                                    c = getchar();
                                }
                                if(c == '\n') {
                                    goto next_option;
                                }
                                for(i = 0; (c != ' ') && (c != '\n'); i++) {
                                    filename[i] = c;
                                    c = getchar();
                                }
                                if(c != '\n') {
                                    while(c == ' ') {
                                        c = getchar();
                                    }
                                    if(c != '\n') {
                                        goto next_option;
                                    }
                                }
                                
                                struct connection *con;
                                if((access(filename, F_OK) == 0) && (access(filename, R_OK) == 0)) {
                                    struct stat file_info;
                                    stat(filename, &file_info);
                                    char *fname = (char *)malloc(PATH_MAX);
                                    memset(fname, 0, PATH_MAX);
                                    sprintf(fname, "%s", filename);
                                    
                                    switch(num) {
                                        case 1:
                                            if(num <= cnct_num) {
                                                con = cnct;
                                                break;
                                            }
                                        case 2:
                                            if(num <= cnct_num) {
                                                con = cnct->next;
                                                break;
                                            }
                                        case 3:
                                            if(num <= cnct_num) {
                                                con = cnct->next->next;
                                                break;
                                            }
                                        case 4:
                                            if(num <= cnct_num) {
                                                con = cnct->next->next->next;
                                                break;
                                            }
                                        case 5:
                                            if(num <= cnct_num) {
                                                con = cnct->next->next->next->next;
                                                break;
                                            }
                                        default:
                                            printf("%i is not a valid connection!\n\n", num);
                                            goto end_upload;
                                    }
                                    
                                    con->fname = fname;
                                    con->fsize = file_info.st_size;
                                    struct timeval tp;
                                    gettimeofday(&tp, NULL);
                                    con->ftime = tp.tv_sec;
                                    con->futime = tp.tv_usec;
                                    
                                    FD_CLR(STDIN_FILENO, &fdset_main);
                                    input_flag = 1;
                                    char dl_response[PATH_MAX + 100];
                                    sprintf(dl_response, "*DOWNLOAD_GRTD %lld %s", file_info.st_size, filename);
                                    send(con->sockfd, dl_response, PATH_MAX + 100, 0);
                                end_upload:
                                    break;
                                }
                                else {
                                    printf("Unable to upload %s to %s!\n", filename, con->hostname);
                                }
                            }
                            else {
                                goto next_option;
                            }
                            break;
                        case DOWNLOAD:
                        download_more:
                            if(c == ' ') {
                                int num;
                                char filename[PATH_MAX];
                                memset(filename, 0, PATH_MAX);
                                
                                while(c == ' ') {
                                    c = getchar();
                                }
                                char c_num[2] = {c, '\0'};
                                num = c != '\n' ? atoi(c_num) : -1;
                                if(num == -1) {
                                    goto next_option;
                                }
                                c = getchar();
                                if(c != ' ') {
                                    goto next_option;
                                }
                                while(c == ' ') {
                                    c = getchar();
                                }
                                if(c == '\n') {
                                    goto next_option;
                                }
                                for(i = 0; (c != ' ') && (c != '\n'); i++) {
                                    filename[i] = c;
                                    c = getchar();
                                }
                                
                                char dl_rqst[PATH_MAX + 17];
                                memset(dl_rqst, 0, PATH_MAX + 17);
                                sprintf(dl_rqst, "*DOWNLOAD_RQST %s", filename);
                                switch(num) {
                                    case 1:
                                        if(num <= cnct_num) {
                                            printf("Downloading %s from connection #%i\n\n", filename, num);
                                            send(cnct->sockfd, dl_rqst, PATH_MAX + 17, 0);
                                            break;
                                        }
                                    case 2:
                                        if(num <= cnct_num) {
                                            printf("Downloading %s from connection #%i\n\n", filename, num);
                                            send(cnct->next->sockfd, dl_rqst, PATH_MAX + 17, 0);
                                            break;
                                        }
                                    case 3:
                                        if(num <= cnct_num) {
                                            printf("Downloading %s from connection #%i\n\n", filename, num);
                                            send(cnct->next->next->sockfd, dl_rqst, PATH_MAX + 17, 0);
                                            break;
                                        }
                                    case 4:
                                        if(num <= cnct_num) {
                                            printf("Downloading %s from connection #%i\n\n", filename, num);
                                            send(cnct->next->next->next->sockfd, dl_rqst, PATH_MAX + 17, 0);
                                            break;
                                        }
                                    case 5:
                                        if(num <= cnct_num) {
                                            printf("Downloading %s from connection #%i\n\n", filename, num);
                                            send(cnct->next->next->next->next->sockfd, dl_rqst, PATH_MAX + 17, 0);
                                            break;
                                        }
                                    default:
                                        printf("%i is not a valid connection!\n\n", num);
                                }
                                
                                if(c != '\n') {
                                    goto download_more;
                                }
                            }
                            else {
                                goto next_option;
                            }
                            break;
                        case CREATOR:
                            if(c == ' ') {
                                while(c != '\n') {
                                    c = getchar();
                                    if((c != ' ') && ( c != '\n')) {
                                        goto next_option;
                                    }
                                }
                            }
                            puts("Creator - Alexander Simeonov\nEmail   - agsimeon@buffalo.edu\n");
                            break;
                        default:
                        next_option:
                            fprintf(stderr, "Invalid option! Please type HELP for a list of available options.\n\n");
                    }
                }
                else { //Connection management
                    int con_num;
                    struct connection *con;
                    switch(cnct_num) { //Get which connection was triggered
                        case 5:
                            if(i == cnct->next->next->next->next->sockfd) {
                                con_num = 5;
                                con = cnct->next->next->next->next;
                                break;
                            }
                        case 4:
                            if(i == cnct->next->next->next->sockfd) {
                                con_num = 4;
                                con = cnct->next->next->next;
                                break;
                            }
                        case 3:
                            if(i == cnct->next->next->sockfd) {
                                con_num = 3;
                                con = cnct->next->next;
                                break;
                            }
                        case 2:
                            if(i == cnct->next->sockfd) {
                                con_num = 2;
                                con = cnct->next;
                                break;
                            }
                        case 1:
                            if(i == cnct->sockfd) {
                                con_num = 1;
                                con = cnct;
                                break;
                            }
                    }
                    
                    char recv_data[(PATH_MAX + 100) > PACKET_SIZE ? (PATH_MAX + 100) : PACKET_SIZE];
                    memset(recv_data, 0, (PATH_MAX + 100) > PACKET_SIZE ? (PATH_MAX + 100) : PACKET_SIZE);
                    int recv_num = recv(con->sockfd, recv_data, (PATH_MAX + 100) > PACKET_SIZE ? (PATH_MAX + 100) : PACKET_SIZE, 0);
                    
                    if(recv_data[0] != '\0') {
                        if(strncmp(recv_data, "-TERMINATE_CONNECTION\n", 22) == 0) {
                            switch(con_num) {
                                case 1:
                                    printf("1: %s has terminated the connection!\n", con->hostname);
                                    FD_CLR(cnct->sockfd, &fdset_main);
                                    shutdown(cnct->sockfd, SHUT_RDWR);
                                    close(cnct->sockfd);
                                    free(cnct->ip);
                                    free(cnct->hostname);
                                        if(cnct_num == 1) {
                                            free(cnct);
                                            cnct = NULL;
                                        }
                                        else {
                                            cnct = cnct->next;
                                            free(cnct->prev);
                                            cnct->prev = NULL;
                                        }
                                    cnct_num = cnct_num - 1;
                                    puts("The connection list is now:");
                                    goto connection_list;
                                case 2:
                                    printf("2: %s has terminated the connection!\n", con->hostname);
                                    FD_CLR(cnct->next->sockfd, &fdset_main);
                                    shutdown(cnct->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->sockfd);
                                    free(cnct->next->ip);
                                    free(cnct->next->hostname);
                                        if(cnct_num == 2) {
                                            free(cnct->next);
                                            cnct->next = NULL;
                                        }
                                        else {
                                            cnct->next = cnct->next->next;
                                            free(cnct->next->prev);
                                            cnct->next->prev = cnct;
                                        }
                                    cnct_num = cnct_num - 1;
                                    puts("The connection list is now:");
                                    goto connection_list;
                                case 3:
                                    printf("3: %s has terminated the connection!\n", con->hostname);
                                    FD_CLR(cnct->next->next->sockfd, &fdset_main);
                                    shutdown(cnct->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->sockfd);
                                    free(cnct->next->next->ip);
                                    free(cnct->next->next->hostname);
                                        if(cnct_num == 3) {
                                            free(cnct->next->next);
                                            cnct->next->next = NULL;
                                        }
                                        else {
                                            cnct->next->next = cnct->next->next->next;
                                            free(cnct->next->next->prev);
                                            cnct->next->next->prev = cnct->next;
                                        }
                                    cnct_num = cnct_num - 1;
                                    puts("The connection list is now:");
                                    goto connection_list;
                                case 4:
                                    printf("4: %s has terminated the connection!\n", con->hostname);
                                    FD_CLR(cnct->next->next->next->sockfd, &fdset_main);
                                    shutdown(cnct->next->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->next->sockfd);
                                    free(cnct->next->next->next->ip);
                                    free(cnct->next->next->next->hostname);
                                        if(cnct_num == 4) {
                                            free(cnct->next->next->next);
                                            cnct->next->next->next = NULL;
                                        }
                                        else {
                                            cnct->next->next->next = cnct->next->next->next->next;
                                            free(cnct->next->next->next->prev);
                                            cnct->next->next->next->prev = cnct->next->next;
                                        }
                                    cnct_num = cnct_num - 1;
                                    puts("The connection list is now:");
                                    goto connection_list;
                                case 5:
                                    printf("5: %s has terminated the connection!\n", con->hostname);
                                    FD_CLR(cnct->next->next->next->next->sockfd, &fdset_main);
                                    shutdown(cnct->next->next->next->next->sockfd, SHUT_RDWR);
                                    close(cnct->next->next->next->next->sockfd);
                                    free(cnct->next->next->next->next->ip);
                                    free(cnct->next->next->next->next->hostname);
                                    free(cnct->next->next->next->next);
                                    cnct->next->next->next->next = NULL;
                                    cnct_num = cnct_num - 1;
                                    puts("The connection list is now:");
                                    goto connection_list;
                            }
                        }//terminates connection
                        else if(strncmp(recv_data, "*DOWNLOAD_RQST", 14) == 0) {
                            char dl_response[PATH_MAX + 100];
                        
                            printf("%i: %s is downloading %s\n", con_num, con->hostname, &recv_data[15]);
                            if((access(&recv_data[15], F_OK) == 0) && (access(&recv_data[15], R_OK) == 0)) {
                                struct stat file_info;
                                stat(&recv_data[15], &file_info);
                                char *filename = (char *)malloc(PATH_MAX);
                                memset(filename, 0, PATH_MAX);
                                sprintf(filename, "%s", &recv_data[15]);
                                con->fname = filename;
                                con->fsize = file_info.st_size;
                                struct timeval tp;
                                gettimeofday(&tp, NULL);
                                con->ftime = tp.tv_sec;
                                con->futime = tp.tv_usec;
                                
                                sprintf(dl_response, "*DOWNLOAD_GRTD %lld %s", file_info.st_size, &recv_data[15]);
                                send(con->sockfd, dl_response, PATH_MAX + 100, 0);
                            }
                            else {
                                sprintf(dl_response, "*DOWNLOAD_DNID %s", &recv_data[15]);
                                send(con->sockfd, dl_response, PATH_MAX + 17, 0);
                                printf("Unable to upload %s to %s!\n", &recv_data[15], con->hostname);
                            }
                        }
                        else if(strncmp(recv_data, "*DOWNLOAD_GRTD", 14) == 0) {//Where downloads were granted
                            char size_string[50];
                            memset(size_string, 0, 50);
                            for(i = 0; recv_data[i + 15] != ' '; i++) {
                                size_string[i] = recv_data[i + 15];
                            }
                            char *filename = (char *)malloc(PATH_MAX);
                            memset(filename, 0, PATH_MAX);char dir_name[6] = {'D', 'L', '\0', '\0', '\0', '\0'};
                            dir_num = dir_num + 1;
                            sprintf(&dir_name[2], "%i", dir_num);
                            sprintf(filename, "%s/%s", dir_name, &recv_data[i + 16]);
                            mkdir(dir_name, (S_IRWXU | S_IRWXG | S_IRWXO));
                            FILE *f = fopen(filename, "w+");
                            fclose(f);
                            con->fname = filename;
                            con->fsize = atoi(size_string);
                            struct timeval tp;
                            gettimeofday(&tp, NULL);
                            con->ftime = tp.tv_sec;
                            con->futime = tp.tv_usec;
                            
                            send(con->sockfd, "DL 1", 4, 0); //Download Segment 1
                        }
                        else if(strncmp(recv_data, "*DOWNLOAD_DNID", 14) == 0) { //Where doenloads were denied
                            printf("Download of %s from %s failed!\n", &recv_data[15], con->hostname);
                        }
                        else if(strncmp(recv_data, "DL", 2) == 0) {
                            int seg_num = atoi(&recv_data[3]);
                            int segsize = (con->fsize - ((seg_num - 1) * PACKET_SIZE)) > PACKET_SIZE ? PACKET_SIZE : (con->fsize - ((seg_num - 1) * PACKET_SIZE));
                            int segment[segsize];
                            memset(segment, 0, segsize);
                            FILE *f = fopen(con->fname, "r");
                            fseek(f, ((seg_num - 1) * PACKET_SIZE), SEEK_SET);
                            fread(segment, segsize, 1, f); 
                            fclose(f);
                            send(con->sockfd, segment, segsize, 0);
                        }
                        else if(strncmp(recv_data, "FINISH", 6) == 0) { //Finish up download
                            printf("Finished uploading %s\n", con->fname);
                            struct timeval tp;
                            gettimeofday(&tp, NULL);
                            printf("Tx (%s): %s -> %s, File Size: %llu Bytes, Time Taken %ld useconds, Tx Rate: %f bits/second\n\n",
                                   hostname, hostname, con->hostname, con->fsize, 
                                   ((tp.tv_sec - con->ftime) * 1000000) + (tp.tv_usec - con->futime),
                                   (con->fsize * 8) / ((((tp.tv_sec - con->ftime) * 1000000) + (tp.tv_usec - con->futime)) * .000001));
                            free(con->fname);
                            if(input_flag == 1) {
                                tcflush(STDIN_FILENO, TCIFLUSH);
                                FD_SET(STDIN_FILENO, &fdset_main);
                            }
                        }
                        else {//Upload
                            struct stat file_info;
                            stat(con->fname, &file_info);
                            int seg_num = (file_info.st_size/PACKET_SIZE) + 1;
                            int segsize = (con->fsize - ((seg_num - 1) * PACKET_SIZE)) > PACKET_SIZE ? PACKET_SIZE : (con->fsize - ((seg_num - 1) * PACKET_SIZE));
                            char dl[25];
                            memset(dl, 0, 25);
                            
                            if(recv_num != segsize) {
                                sprintf(dl, "DL %i", seg_num);
                                send(con->sockfd, dl, 25, 0);
                            }
                            else {
                                FILE *f = fopen(con->fname, "a");
                                fwrite(recv_data, segsize, 1, f);
                                fclose(f);
                                
                                stat(con->fname, &file_info);
                                if(file_info.st_size == con->fsize) {
                                    printf("Finished downloading %s\n", con->fname);
                                    struct timeval tp;
                                    gettimeofday(&tp, NULL);
                                    printf("Rx (%s): %s -> %s, File Size: %llu Bytes, Time Taken %ld useconds, Rx Rate: %f bits/second\n\n",
                                           hostname, con->hostname, hostname, con->fsize, 
                                           ((tp.tv_sec - con->ftime) * 1000000) + (tp.tv_usec - con->futime),
                                           (con->fsize * 8) / ((((tp.tv_sec - con->ftime) * 1000000) + (tp.tv_usec - con->futime)) * .000001));
                                    send(con->sockfd, "FINISH", 6, 0);
                                    free(con->fname);
                                }
                                else {
                                    sprintf(dl, "DL %i", (seg_num + 1));
                                    send(con->sockfd, dl, 25, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
