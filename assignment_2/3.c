#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

//routing table
struct rtable {
    int num_s, num_n, host_index;
    int *id, *nhop, *port, *nghbr;
    char **ip, **cost, **ncost;
};

//aquires host ip
char *getmyip() {
    char *myip = (char *)malloc(INET_ADDRSTRLEN*sizeof(char));
    memset(myip, '\0', INET_ADDRSTRLEN);
    int sockfd;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("8.8.8.8");
    address.sin_port = htons(53);
    socklen_t address_len = sizeof(address);
    
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("getmyip() - socket() error");
        exit(1);
    }
    if((connect(sockfd, (const struct sockaddr *)&address, sizeof(address))) == -1) {
        perror("getmyip() - connect() error");
        exit(1);
    }
    memset(&address, 0, sizeof(address));
    getsockname(sockfd, (struct sockaddr *) &address, &address_len);
    inet_ntop(AF_INET, &address.sin_addr, myip, INET_ADDRSTRLEN);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return myip;
}

//constructs rtable
struct rtable *make_rtable(char *filename, char *myip) {
    FILE *f = fopen(filename, "r");
    struct rtable *rt = (struct rtable *)malloc(sizeof(struct rtable));
    rt->host_index = -1;
    int *id = (int *)malloc(5*sizeof(int));
    int *nhop = (int *)malloc(5*sizeof(int));
    int *port = (int *)malloc(5*sizeof(int));
    int *nghbr = (int *)malloc(5*sizeof(int));
    nghbr[0] = 0; nghbr[1] = 0; nghbr[2] = 0; nghbr[3] = 0; nghbr[4] = 0; 
    char **ip = (char **)malloc(5*sizeof(char *));
    ip[0], ip[1], ip[2], ip[3], ip[4] = NULL;
    char **cost = (char **)malloc(5*sizeof(char *));
    char **ncost = (char **)malloc(5*sizeof(char *));
    cost[0], cost[1], cost[2], cost[3], cost[4] = NULL;
    
    char c;
    while(((c = fgetc(f)) == ' ') || (c == '\n')) {}
    char ns[2] = {c, '\0'};
    rt->num_s = atoi(ns);
    if(rt->num_s > 5) {
        fprintf(stderr, "can't handle more then five servers\n");
        exit(1);
    }
    while((c = fgetc(f)) != '\n') {}
    while(((c = fgetc(f)) == ' ') || (c == '\n')) {}
    char nn[2] = {c, '\0'};
    rt->num_n = atoi(nn);
    while((c = fgetc(f)) != '\n') {}
    int i, j;
    for(i = 0; i < rt->num_s; i++) {
        while(((c = fgetc(f)) == ' ') || (c == '\n')) {}
        char cur_id[9] = {c,'\0','\0','\0','\0','\0','\0','\0','\0'};
        for(j = 1; (c = fgetc(f)) != ' '; j++) {
            cur_id[j] = c;
        }
        id[i] = atoi(cur_id);
        while((c = fgetc(f)) == ' ') {}
        char *cur_ip = (char *)malloc(INET_ADDRSTRLEN*sizeof(char));
        cur_ip[0] = c;
        for(j = 1; (c = fgetc(f)) != ' '; j++) {
            cur_ip[j] = c;
        }
        cur_ip[j] = '\0';
        while((c = fgetc(f)) == ' ') {}
        char p[6] = {'\0','\0','\0','\0','\0','\0'};
        p[0] = c;
        for(j = 1; ((c = fgetc(f)) != '\n') && (c != EOF); j++) {
            p[j] = c;
        }
        ip[i] = cur_ip;
        port[i] = atoi(p);
    }
    while((c != EOF) && (rt->num_n != 0)) {
        while(((c = fgetc(f)) == ' ') || (c == '\n')) {}
        if(c == EOF) {
            break;
        }
        char h_id[9] = {c,'\0','\0','\0','\0','\0','\0','\0','\0'};
        for(j = 1; (c = fgetc(f)) != ' '; j++) {
            h_id[j] = c;
        }
        while((c = fgetc(f)) == ' ') {}
        char n_id[9] = {c,'\0','\0','\0','\0','\0','\0','\0','\0'};
        for(j = 1; (c = fgetc(f)) != ' '; j++) {
            n_id[j] = c;
        }
        while((c = fgetc(f)) == ' ') {}
        char *cur_cost = (char *)malloc(9*sizeof(char));
        char *cur_ncost = (char *)malloc(9*sizeof(char));
        cur_cost[0] = c;
        cur_ncost[0] = c;
        c = fgetc(f);
        for(j = 1; (c != ' ') && (c != '\n') && (c != EOF) ; j++) {
            cur_ncost[j] = c;
            cur_cost[j] = c;
            c = fgetc(f);
        }
        cur_cost[j] = '\0';
        cur_ncost[j] = '\0';
        while((c != '\n') && (c != EOF)) {
            c = fgetc(f);
        }
        if(rt->host_index == -1) {
            for(j = 0; j < 5; j++) {
                if(strcmp(ip[j], myip) == 0) {
                    rt->host_index = j;
                    break;
                }
            }
            if(strcmp(ip[rt->host_index], myip) != 0) {
                continue;
            }
        }
        if(id[rt->host_index] != atoi(h_id)) {
            continue;
        }
        for(j = 0; j < 5; j++) {
            if(id[j] == atoi(n_id)) {
                cost[j] = cur_cost;
                ncost[j] = cur_ncost;
                break;
            }
        }
    }
    for(j = 0; j < rt->num_s; j++) {
        if(cost[j] == NULL) {
            if(j == rt->host_index) {
                char *cur_cost = (char *)malloc(2*sizeof(char));
                char *cur_ncost = (char *)malloc(2*sizeof(char));
                cur_cost[0] = '0'; cur_cost[1] = '\0';
                cur_ncost[0] = '0'; cur_ncost[1] = '\0';
                cost[j] = cur_cost;
                ncost[j] = cur_ncost;
            }
            else {
                char *cur_cost = (char *)malloc(4*sizeof(char));
                char *cur_ncost = (char *)malloc(4*sizeof(char));
                cur_cost[0] = 'i'; cur_cost[1] = 'n'; cur_cost[2] = 'f'; cur_cost[3] = '\0';
                cur_ncost[0] = 'i'; cur_ncost[1] = 'n'; cur_ncost[2] = 'f'; cur_ncost[3] = '\0';
                cost[j] = cur_cost;
                ncost[j] = cur_ncost;
            }
            nhop[j] = INT_MIN;
        }
        else {
            nhop[j] = id[j];
            nghbr[j] = 1;
        }
    }
    rt->id = id;
    rt->nhop = nhop;
    rt->port = port;
    rt->nghbr = nghbr;
    rt->ip = ip;
    rt->cost = cost;
    rt->ncost = ncost;
    fclose(f);
    return rt;
}

//aquires index of server by ID
int getindex(int id, int *server, int size) {
    int i;
    for(i = 0; i <= size; i++) {
        if(server[i] == id) {
            break;
        }
    }
    return i;
}

//aquires index of server by ip and port
int getindexip(char *ip, int port, struct rtable *rt) {
    int i;
    for(i = 0; i <= rt->num_s; i++) {
        if(!strcmp(ip, rt->ip[i])) {
            if(port == rt->port[i]) {
                break;
            }
        }
    }
    return i;
}

//generates the message
uint16_t *create_msg(struct rtable *rt, uint16_t *m) {
    m[0] = (uint16_t)rt->num_s;
    m[1] = (uint16_t)rt->port[rt->host_index];
    inet_pton(AF_INET, rt->ip[rt->host_index], &m[2]);
    switch(rt->num_s) {
        case 5:
            inet_pton(AF_INET, rt->ip[4], &m[28]);
            m[30] = (uint16_t)rt->port[4];
            m[31] = (uint16_t)rt->id[4];
            m[32] = !strcmp(rt->cost[4], "inf") ? 0xFFFF : (uint32_t)atoi(rt->cost[4]) >> 16;
            m[33] = !strcmp(rt->cost[4], "inf") ? 0xFFFF : ((uint32_t)atoi(rt->cost[4]) << 16) >> 16;;
        case 4:
            inet_pton(AF_INET, rt->ip[3], &m[22]);
            m[24] = (uint16_t)rt->port[3];
            m[25] = (uint16_t)rt->id[3];
            m[26] = !strcmp(rt->cost[3], "inf") ? 0xFFFF : (uint32_t)atoi(rt->cost[3]) >> 16;
            m[27] = !strcmp(rt->cost[3], "inf") ? 0xFFFF : ((uint32_t)atoi(rt->cost[3]) << 16) >> 16;
        case 3:
            inet_pton(AF_INET, rt->ip[2], &m[16]);
            m[18] = (uint16_t)rt->port[2];
            m[19] = (uint16_t)rt->id[2];
            m[20] = !strcmp(rt->cost[2], "inf") ? 0xFFFF : (uint32_t)atoi(rt->cost[2]) >> 16;
            m[21] = !strcmp(rt->cost[2], "inf") ? 0xFFFF : ((uint32_t)atoi(rt->cost[2]) << 16) >> 16;
        case 2:
            inet_pton(AF_INET, rt->ip[1], &m[10]);
            m[12] = (uint16_t)rt->port[1];
            m[13] = (uint16_t)rt->id[1];
            m[14] = !strcmp(rt->cost[1], "inf") ? 0xFFFF : (uint32_t)atoi(rt->cost[1]) >> 16;
            m[15] = !strcmp(rt->cost[1], "inf") ? 0xFFFF : ((uint32_t)atoi(rt->cost[1]) << 16) >> 16;
        case 1:
            inet_pton(AF_INET, rt->ip[0], &m[4]);
            m[6] = (uint16_t)rt->port[0];
            m[7] = (uint16_t)rt->id[0];
            m[8] = !strcmp(rt->cost[0], "inf") ? 0xFFFF : (uint32_t)atoi(rt->cost[0]) >> 16;
            m[9] = !strcmp(rt->cost[0], "inf") ? 0xFFFF : ((uint32_t)atoi(rt->cost[0]) << 16) >> 16;
    }
    return m;
}

int main(int argc, char *argv[]) {
    struct rtable *rt;
    struct timeval routing_intv;
    int interval[5] = {0,0,0,0,0};
    int disable[5] = {0,0,0,0,0};
    int sockfd, nfds;
    int packets, crash, itv = 0;
    fd_set fdset_main;
    
    //startup command setup
    if(argc == 5) {
        char *myip = getmyip();
        if(!strcmp(argv[1], "-t")) {
            rt = make_rtable(argv[2], myip);
        }
        else if(!strcmp(argv[3], "-t")) {
            rt = make_rtable(argv[4], myip);
        }
        else {
            goto inv_cmd;
        }
        free(myip);
        if(!strcmp(argv[1], "-i")) {
            routing_intv.tv_sec = atoi(argv[2]);
             
        }
        else if(!strcmp(argv[3], "-i")) {
            routing_intv.tv_sec = atoi(argv[4]);
        }
        else {
            goto inv_cmd;
        }
        routing_intv.tv_usec = 0;
    }
    else {
    inv_cmd:
        fprintf(stderr, "invalid command line parameters\n");
        exit(1);
    }
    
    //server setup
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket() error");
        exit(1);
    }
    struct sockaddr_in saddr;
    memset((void *)&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(rt->port[rt->host_index]);
    if(bind(sockfd, (const struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        perror("bind() error");
        exit(1);
    }
    
    //select setup
    FD_ZERO(&fdset_main);
    FD_SET(sockfd, &fdset_main);
    FD_SET(STDIN_FILENO, &fdset_main);
    nfds = sockfd > STDIN_FILENO ? sockfd + 1 : STDIN_FILENO + 1;
    
    while(1) {
        fd_set fdset = fdset_main;
        struct timeval ri = routing_intv;
        if(select(nfds+1, &fdset, NULL, NULL, &ri) == -1) {
            perror("select() error");
            exit(1);
        }
        
        int i, j;
        for(i = 0; i <= nfds; i++) {
            if(FD_ISSET(i, &fdset)) {
                //command line
                if(i == STDIN_FILENO) {
                    char opt[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
                    char c = tolower(getchar());
                    for(j = 0; (c != ' ') && (c != '\n') && (j < 8); j++) {
                        opt[j] = c;
                        c = tolower(getchar());
                    }
                    if(j == 8) {
                        puts("invalid option");
                    }
                    
                    if(!strcmp(opt, "update")) {
                        if(c == '\n') {
                            puts("40 <update> <invlaid parameters>");
                        }
                        else {
                            while(c == ' ') {
                                c = tolower(getchar());
                            }
                            char id1[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
                            for(j = 0; (c != '\n') && (c != ' ') && (j < 9); j++) {
                                id1[j] = c;
                                c = tolower(getchar());
                            }
                            if((j == 9) || (c == '\n')) {
                                puts("40 <update> <invlaid parameters>");
                            }
                            else {
                                while(c == ' ') {
                                    c = tolower(getchar());
                                }
                                char id2[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
                                for(j = 0; (c != '\n') && (c != ' ') && (j < 9); j++) {
                                    id2[j] = c;
                                    c = tolower(getchar());
                                }
                                if((j == 9) || (c == '\n')) {
                                    puts("40 <update> <invlaid parameters>");
                                }
                                else {
                                    while(c == ' ') {
                                        c = tolower(getchar());
                                    }
                                    char *cst = (char *)malloc(9*sizeof(char));
                                    for(j = 0; (c != '\n') && (c != ' ') && (j < 9); j++) {
                                        cst[j] = c;
                                        c = tolower(getchar());
                                    }
                                    if(j == 9) {
                                        free(cst);
                                        puts("40 <update> <invlaid parameters>");
                                    }
                                    else {
                                        cst[j] = '\0';
                                        int index1 = getindex(atoi(id1), rt->id, rt->num_s);
                                        int index2 = getindex(atoi(id2), rt->id, rt->num_s);
                                        if((index1 == (rt->num_s + 1)) || (index1 != rt->host_index)) {
                                            puts("40 <update> <invalid host-ID>");
                                        }
                                        else if(index2 == (rt->num_s + 1)) {
                                            puts("40 <update> <invalid server-ID>");
                                        }
                                        else if(!rt->nghbr[index2]) {
                                            puts("40 <update> <not a neighbor>");
                                        }
                                        else if(disable[index2]) {
                                            puts("40 <update> <disabled>");
                                        }
                                        else {
                                            int cst_old, cst_new;
                                            if(strcmp(cst, "inf") != 0) {
                                                int cst_new = atoi(cst);
                                                if(strcmp(rt->cost[index2], "inf") != 0) {
                                                    cst_old = atoi(rt->cost[index2]);
                                                }
                                                else {
                                                    cst_old = INT_MAX;
                                                }
                                            }
                                            else {
                                                cst_old, cst_new = INT_MAX;
                                            }
                                            if((rt->id[index2] == rt->nhop[index2]) ||
                                               (cst_new < cst_old)) {
                                                char *dcst = (char *)malloc(9*sizeof(char));
                                                for(j = 0; j < 9; j++) {
                                                    dcst[j] = cst[j];
                                                }
                                                free(rt->cost[index2]);
                                                rt->cost[index2] = dcst;
                                                rt->nhop[index2] = rt->id[index2];
                                            }
                                            free(rt->ncost[index2]);
                                            rt->ncost[index2] = cst;
                                            ri.tv_sec = 0;
                                            ri.tv_usec = 0;
                                            puts("40 <update> SUCCESS");
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if(!strcmp(opt, "step")) {
                        ri.tv_sec = 0;
                        ri.tv_usec = 0;
                        puts("40 <step> SUCCESS");
                    }
                    else if(!strcmp(opt, "packets")) {
                        printf("# routing updates: %i\n", packets);
                        packets = 0;
                        puts("40 <packets> SUCCESS");
                    }
                    else if(!strcmp(opt, "display")) {
                        int order[rt->num_s];
                        int id[5] = {rt->id[0],rt->id[1],rt->id[2],rt->id[3],rt->id[4]};
                        for(j = 0; j < rt->num_s; j++) {
                            int h, indx, val = INT_MAX;
                            for(h = 0; h < rt->num_s; h++) {
                                if(id[h] < val) {
                                    val = id[h];
                                    indx = h;
                                }
                            }
                            order[j] = indx;
                            id[indx] = INT_MAX;
                        }
                        for(j = 0; j < rt->num_s; j++) {
                            if(crash) {}
                            else if(j == rt->host_index) {
                                if(rt->nghbr[rt->host_index]) {
                                    printf("<%i> <%i> <%s>\n", rt->id[order[j]], rt->nhop[order[j]], 
                                           rt->cost[getindex(rt->id[order[j]], rt->id, rt->num_s)]);
                                }
                                else {
                                    printf("<%i> <%i> <0>\n", rt->id[rt->host_index],
                                           rt->id[rt->host_index]);
                                }
                            }
                            else if(strcmp(rt->cost[order[j]], "inf")) {
                                printf("<%i> <%i> <%s>\n", rt->id[order[j]], rt->nhop[order[j]], 
                                       rt->cost[getindex(rt->id[order[j]], rt->id, rt->num_s)]);
                            }
                            else {
                                printf("<%i> <N/A> <inf>\n", rt->id[order[j]]);
                            }
                        }
                        puts("40 <display> SUCCESS");
                    }
                    else if(!strcmp(opt, "disable")) {
                        if(c == '\n') {
                            puts("40 <disable> <invlaid server-ID>");
                        }
                        else {
                            while(c == ' ') {
                                c = tolower(getchar());
                            }
                            char id[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
                            for(j = 0; (c != '\n') && (c != ' ') && (j < 9); j++) {
                                id[j] = c;
                                c = tolower(getchar());
                            }
                            if(j == 9) {
                                puts("40 <disable> <invlaid server-ID>");
                            }
                            else {
                                int index = getindex(atoi(id), rt->id, rt->num_s);
                                if(index == (rt->num_s + 1)) {
                                    puts("40 <disable> <invalid server-ID>");
                                }
                                else if(!rt->nghbr[index]) {
                                    puts("40 <disable> <not a neighbor>");
                                }
                                else if(disable[index]) {
                                    puts("40 <disable> <already disabled>");
                                }
                                else {
                                    char *cur_cost = (char *)malloc(4*sizeof(char));
                                    cur_cost[0] = 'i'; cur_cost[1] = 'n'; 
                                    cur_cost[2] = 'f'; cur_cost[3] = '\0';
                                    char *cur_ncost = (char *)malloc(4*sizeof(char));
                                    cur_ncost[0] = 'i'; cur_ncost[1] = 'n'; 
                                    cur_ncost[2] = 'f'; cur_ncost[3] = '\0';
                                    if(rt->nhop[index] == rt->id[index]) {
                                        free(rt->cost[index]);
                                        rt->cost[index] = cur_cost;
                                        rt->nhop[index] = INT_MIN;
                                    }
                                    free(rt->ncost[index]);
                                    rt->ncost[index] = cur_ncost;
                                    disable[index] = 1;
                                    puts("40 <disable> SUCCESS");
                                }
                            }
                        }
                    }
                    else if(!strcmp(opt, "crash")) {
                        crash = 1;
                        for(j = 0; j < rt->num_s; j++) {
                            char *cur_cost = (char *)malloc(4*sizeof(char));
                            cur_cost[0] = 'i'; cur_cost[1] = 'n'; cur_cost[2] = 'f'; cur_cost[3] = '\0';
                            free(rt->cost[j]);
                            rt->cost[j] = cur_cost;
                            rt->nhop[j] = INT_MIN;
                            disable[j] = 1;
                        }
                        puts("40 <crash> SUCCESS");
                    }
                    else {
                        puts("invalid option");
                    }
                }
                //interval updates server
                else if((i == sockfd) && (crash == 0)) {
                    packets = packets + 1;
                    uint16_t m[34];
                    int k = recvfrom(sockfd, m, 34*2, 0,(struct sockaddr *)&saddr, NULL);
                    char remote_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &m[2], remote_ip, INET_ADDRSTRLEN);
                    int remote_idx = getindexip(remote_ip, m[1], rt);
                    printf("received update from server-ID: %i\n", rt->id[remote_idx]);
                    for(j = 0; j < m[0]; j++) {
                        char loc_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &m[4+(6*j)], loc_ip, INET_ADDRSTRLEN);
                        int loc_idx = getindexip(loc_ip, m[6+(6*j)], rt);
                        uint32_t cst = 0;
                        cst = cst | ((uint32_t)m[8+(6*j)] << 16);
                        cst = cst | (uint32_t)m[9+6*j];
                        int cst_old, cst_new;
                        if(cst != 0xFFFFFFFF) {
                            cst_new = cst;
                            if(strcmp(rt->cost[loc_idx], "inf") != 0) {
                                cst_old = atoi(rt->cost[loc_idx]);
                            }
                            else {
                                cst_old = INT_MAX;
                            }
                        }
                        else {
                            cst_old, cst_new = INT_MAX;
                        }
                        if((rt->host_index == loc_idx) && (disable[remote_idx] == 0)) {
                            char *ucst = (char *)malloc(10*sizeof(char));
                            sprintf(ucst, "%i\0", cst_new);
                            free(rt->ncost[remote_idx]);
                            rt->ncost[remote_idx] = ucst;
                            if(rt->nhop[remote_idx] == rt->id[remote_idx]) {
                                char *ucst2 = (char *)malloc(10*sizeof(char));
                                sprintf(ucst2, "%i\0", cst_new);
                                free(rt->cost[remote_idx]);
                                rt->cost[remote_idx] = ucst2;
                            }
                        }
                    }
                    for(j = 0; j < m[0]; j++) {
                        char loc_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &m[4+(6*j)], loc_ip, INET_ADDRSTRLEN);
                        int loc_idx = getindexip(loc_ip, m[6+(6*j)], rt);
                        uint32_t cst = 0;
                        cst = cst | ((uint32_t)m[8+(6*j)] << 16);
                        cst = cst | (uint32_t)m[9+6*j];
                        int cst_old, cst_new;
                        if(cst != 0xFFFFFFFF) {
                            cst_new = cst;
                            if(strcmp(rt->cost[loc_idx], "inf") != 0) {
                                cst_old = atoi(rt->cost[loc_idx]);
                            }
                            else {
                                cst_old = INT_MAX;
                            }
                        }
                        else {
                            cst_old, cst_new = INT_MAX;
                        }
                        if((rt->host_index == loc_idx) && (disable[remote_idx] == 0)) {
                            continue;
                        }
                        else {
                            if(cst_new != INT_MAX) {
                                cst_new = cst_new + atoi(rt->ncost[remote_idx]);
                                if((cst_new < cst_old)) {
                                    char *ucst = (char *)malloc(10*sizeof(char));
                                    sprintf(ucst, "%i\0", cst_new);
                                    free(rt->cost[loc_idx]);
                                    rt->cost[loc_idx] = ucst;
                                    rt->nhop[loc_idx] = rt->id[remote_idx];
                                }
                            }
                        }
                    }
                }
            }
        }
        //interval updates client
        if((ri.tv_sec == 0) && (ri.tv_usec == 0) && (crash == 0)) {
            for(i = 0; i < rt->num_s; i++) {
                if(rt->nghbr[i] && !disable[i] && (strcmp(rt->cost[i], "inf") != 0)) {
                    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
                    struct sockaddr_in sendaddr;
                    memset((void *)&saddr, 0, sizeof(sendaddr));
                    sendaddr.sin_family = AF_INET;
                    sendaddr.sin_addr.s_addr = inet_addr(rt->ip[i]);
                    sendaddr.sin_port = htons(rt->port[i]);
                    int size = ((4+(rt->num_s*6))*2);
                    uint16_t m[4+(rt->num_s*6)];
                    create_msg(rt, m);
                    sendto(sendfd, m, size, 0,
                           (const struct sockaddr *)&sendaddr, sizeof(sendaddr));
                    shutdown(sendfd, SHUT_RDWR);
                    close(sendfd);
                }
            }
        }
    }
}
