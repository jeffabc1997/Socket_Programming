#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// #include <netinet/in.h>
#include <netdb.h>
#define BUFFER_SIZE 1024
#define INPUT_SIZE 256
#define PORT 80


int Url_end_index = 0;
char Url_link[BUFFER_SIZE];

struct Link {
    char buffer[BUFFER_SIZE];
    int string_len;
    struct Link* next;
};
/* need a global space for links */
struct Link* answer = NULL;
struct Link* ptr = NULL;

char Href_check[6];
int Tag_count = 0;
int Href_count = 0;

void lab2(char buffer[], int buffer_len) {
    int buf_idx = 0;
    while(buf_idx < buffer_len) {
        if(Tag_count == 2) {
            if(buffer[buf_idx] == '>') { // <a ... >
                Tag_count = 0;
            } else{
                if(Href_count == 6) {
                    // search link until {"}
                    if(buffer[buf_idx] == '\"') { // <a ... href=""
                        // save the url link in global
                        struct Link* tmp = (struct Link*) malloc(sizeof(struct Link));
                        if(answer == NULL) { // start from head
                            answer = tmp;
                            ptr = answer;
                        } else { // add from tail
                            ptr->next = tmp;
                            ptr = ptr->next;
                        }
                        ptr->next = NULL;
                        strncpy(ptr->buffer, Url_link, Url_end_index);
                        ptr->string_len = Url_end_index;
                        Url_end_index = 0;
                        // terminate
                        Href_count = 0;
                        memset(Href_check, '\0', 6 * sizeof(char));
                    } else {
                        // add char into URL LINK array
                        Url_link[Url_end_index++] = buffer[buf_idx];
                    }
                } else {
                    // search whole {href="}
                    // use sliding window, seems okay in this else statement block
                    Href_check[Href_count++] = buffer[buf_idx];
                    if((Href_count == 1 && Href_check[0] != 'h') || (Href_count == 2 && strcmp(Href_check, "hr") != 0) ||
                        (Href_count == 3 && strcmp(Href_check, "hre") != 0) || (Href_count == 4 && strcmp(Href_check, "href") != 0) ||
                        (Href_count == 5 && strcmp(Href_check, "href=") != 0) || (Href_count == 6 && strcmp(Href_check, "href=\"") != 0)) {
                        Href_count = 0;
                        memset(Href_check, '\0', 6 * sizeof(char));
                    }
                }
            } 
        } else if(Tag_count == 1){ // <?
            /* search a for <a */
            if(buffer[buf_idx] == 'a') {
                Tag_count = 2;
            } else {
                Tag_count = 0;
            }
        } else if(Tag_count == 0 && buffer[buf_idx] == '<') {
            /* search < for <a */
            Tag_count = 1;            
        }
        buf_idx++;
    }
}

void separate_host_path(char webname[], char hostname[], char pathname[]) {

    int input_len = strlen(webname), idx = 0;
    while((webname[idx] != '/') && (idx < input_len)){
        idx++;
    }
    strncpy(hostname, webname, idx); // copy from start of the input until '/'
    int path_start_idx = ++idx;
    while(idx < input_len) {
        idx++;
    }
    if(path_start_idx < input_len) { // if '/' exists then copy path
        strncpy(&pathname[1], &webname[path_start_idx], (input_len - path_start_idx));
    }
}

/* ref: https://beej-zhtw.netdpi.net/05-system-call-or-bust/5-1-getaddrinfo-start */
void find_ip(char hostname[], char ipname[]) {
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET 或 AF_INET6 可以指定版本
    hints.ai_socktype = SOCK_STREAM;
    // char hostname[] = "can.cs.nthu.edu.tw";
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        // return 2;
    }

    // printf("IP addresses for %s:\n\n", hostname);
    for(p = res;p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        // 取得本身位址的指標，
        // 在 IPv4 與 IPv6 中的欄位不同：
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        // printf(" %s: %s\n", ipver, ipstr); // debug
        strncpy(ipname, ipstr, strlen(ipstr)); // store in output
    }
    
    freeaddrinfo(res); // 釋放鏈結串列
}
int main() { 
    
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    
    char webname[INPUT_SIZE];
    char hostname[INPUT_SIZE] = {'\0'};
    char pathname[INPUT_SIZE] = {'\0'};
    char ipname[64] = {'\0'}; // 140.114.xx.xx
    char get_message[INPUT_SIZE*3];
    char buffer[BUFFER_SIZE] = {'\0'}; // receiving buffer
    pathname[0] = '/'; // path always start with /
    printf("Please enter the URL:\n");
    scanf("%s", webname); // input hostname + path
    /* Handle input into host and path, and find ip address */
    separate_host_path(webname, hostname, pathname); // hostname and pathname will be set in this line
    find_ip(hostname, ipname); // set ip in ipname

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ipname); // ip addr
    server_addr.sin_port = htons(PORT);
    printf("======== Socket ========\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket()");
        exit(EXIT_FAILURE);
    } 
    if (connect(sockfd, (struct sockaddr *)&server_addr, addrlen) == -1) {
        perror("connect()");
        fprintf(stderr, "Please start the server first\n");
        exit(EXIT_FAILURE);
    } 
    printf("Sending HTTP request\n");
    // char *message = "GET / HTTP/1.1\r\nHost: can.cs.nthu.edu.tw\r\nConnection: close\r\n\r\n"; //can.cs.nthu.edu.tw debug
    sprintf(get_message, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", pathname, hostname); // put strings into 1 GET message
    // printf("%s....\n", get_message); // debug
    send(sockfd, get_message, strlen(get_message), 0);
    printf("Receiving the response\n");
    int recv_count = recv(sockfd, buffer, BUFFER_SIZE, 0); // recv_count shows how many bytes we receive
    int count = 0; // for debug
    
    // Consider using recv() in a loop for large data to ensure complete message reception
    while(recv_count > 0) {
        
        lab2(buffer, recv_count);
        recv_count = recv(sockfd, buffer, BUFFER_SIZE, 0); // Idea: https://stackoverflow.com/questions/30470505/http-request-using-sockets-in-c
    }
    ptr = answer;
    int ttl_href_count = 0;
    printf("======== Hyperlinks ========\n");
    while(ptr) {
        printf("%.*s\n", ptr->string_len, ptr->buffer);
        ttl_href_count++;
        ptr = ptr->next;
    }
    printf("================================\nWe have found %i hyperlinks\n", ttl_href_count);

    close(sockfd);
    return 0; 
} 