#include "lab.h"
#include <time.h>

struct Window {
    int cur_seq;
    int current;
    int send_base;
    int available;
    int acked;
} ;
struct Window Cwnd;

bool* Ack_field;
int Ack_size;
pthread_mutex_t mutex;
void printServerInfo(unsigned short port) {
    printf("═══════ Server ═══════\n");
    printf("Server IP is 127.0.0.1\n");
    printf("Listening on port %hu\n", port);
    printf("══════════════════════\n");
}

void sendMessage(char *message) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));

    packet.header.size = strlen(message);
    packet.header.isLast = true;
    strcpy((char *)packet.data, message);

    if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientInfo, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }
}

void recvCommand(char *command) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));

    if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientInfo, (socklen_t *)&addrlen) == -1) {
        perror("recvfrom()");
        exit(EXIT_FAILURE);
    }

    strncpy(command, (char *)packet.data, packet.header.size);
}

FILE *getFile(char *filename) {
    FILE *fd = fopen(filename, "rb");
    return fd;
}

size_t getFileSize(FILE *fd) {
    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    return size;
}
bool check_available(int avai, Packet* send) { // this function is in critical section
    if(avai < Cwnd.send_base + WINDOW_SIZE) { 
        send->header.seq = Cwnd.available++; // also increment Global Available
        return true;
    } else { // no seq is available
        return false;
    }
}
bool check_timeout(struct timespec tt1, struct timespec tt2, unsigned int seq) {
    int first_time = tt1.tv_sec * 1000000000 + tt1.tv_nsec;
    int second_time = tt2.tv_sec * 1000000000 + tt2.tv_nsec;
    if(second_time - first_time > 100000000) {
        printf("Timeout! Resend! %u \n", seq);
        return true;
    } else {
        return false;
    }
}
void sendPacket(FILE *fd, Packet* send, int filesize) {
    // Seek to the "current" position in the file
    int current = send->header.seq * MTU;
    fseek(fd, current, SEEK_SET);
        
    // Read 1024 bytes from the file into the data field of the packet we will send
    fread(send->data, sizeof(char), MTU, fd);

    // Check if the current position indicates that the last packet is to be sent
    if(current + MTU >= filesize) {
        send->header.size = filesize - current;// If it is, set the packet size to the remaining bytes in the file
        send->header.isLast = true; // Set the isLast flag to true
    } else { // Otherwise
        send->header.size = MTU; // Set the packet size to 1024
        send->header.isLast = false;
    }
    
    printf("Send SEQ = %u\n", send->header.seq);
    // Send the packet to the client
    struct timespec tt1, tt2;
    if (sendto(sockfd, send, sizeof(*send), 0, (struct sockaddr *)&clientInfo, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }
    clock_gettime(CLOCK_REALTIME, &tt1);
    /* CHECK ACK FIELD */
    while(Ack_field[send->header.seq] == false) { // check if Acked
        clock_gettime(CLOCK_REALTIME, &tt2);
        if(check_timeout(tt1, tt2, send->header.seq)) { // need to translate second to millisecond 
            if (sendto(sockfd, send, sizeof(*send), 0, (struct sockaddr *)&clientInfo, sizeof(struct sockaddr_in)) == -1) { // retransmit
                perror("sendto()");
                exit(EXIT_FAILURE);
            }
            printf("Send SEQ = %u\n", send->header.seq);
            clock_gettime(CLOCK_REALTIME, &tt1);
        }
    }
}
void* sendFile(void *param) {
    // myParam* args = (myParam*) param;
    char* str = (char*) param;
    // FILE* fd = (FILE*) param; // need param
    FILE* fd = getFile(str);
    size_t filesize = getFileSize(fd);
    Packet send;
    // Set all fields in the packet to 0, including header.seq, header.ack, header.size, header.isLast, and data payload fields
    memset(&send, 0, sizeof(send));
    
    int avai = 0;
    bool oktosend = false;
    
    while(1) {
        pthread_mutex_lock(&mutex);
        if(Cwnd.available < Ack_size) { // not finished sending
            oktosend = check_available(Cwnd.available, &send); // something in the window not yet sent
        } else { 
            pthread_mutex_unlock(&mutex); // we still need to unlock mutex before exit loop
            break; // all packets sent 
        }
        pthread_mutex_unlock(&mutex);
        if(oktosend) { // not ok then search for availabe seq again
            sendPacket(fd, &send, filesize);
            oktosend = false;
        }
    }
    fclose(fd);
    pthread_exit(NULL);
}
void recvAck() { /* maybe no need for a new thread */
    
    Packet recv;
    memset(&recv, 0, sizeof(recv));
    int ack_count = Ack_size;
    int ack_num;

    while(ack_count > 0) {
        if (recvfrom(sockfd, &recv, sizeof(recv), 0, (struct sockaddr *)&clientInfo, (socklen_t *)&addrlen) == -1) {
            perror("recvfrom(client)");
            exit(EXIT_FAILURE);
        }
            
        printf("Received ACK = %u\n", recv.header.ack);
        ack_num = recv.header.ack;
        /* Actually we need a mechanism to check if the ack is in some window range */
        if((ack_num < Cwnd.send_base + WINDOW_SIZE) && (Ack_field[ack_num] == false)) {
            Ack_field[ack_num] = true;
            ack_count--;
            if(ack_num == Cwnd.send_base) { // oldest ack arrives, base+1
                pthread_mutex_lock(&mutex);
                while(Ack_field[Cwnd.send_base] == true) { // checking consecutive oldest acks
                    Cwnd.send_base++;
                }
                pthread_mutex_unlock(&mutex);
            }
        } else if(ack_num < Cwnd.send_base) {
            // if receive older ack, just ignore
        } 
        /* -- */ 
    }
    // pthread_exit(NULL); // if this not a thread, then no need to pthread_exit
}
void selective_repeat(char* str) {
    // FILE* fd = getFile(str);
    pthread_t t[WINDOW_SIZE]; // 宣告 pthread 變數

    for(int i = 0; i < WINDOW_SIZE; i++) {
        pthread_create(&t[i], NULL, sendFile, (void*) str); // 建立子執行緒
    }
    recvAck();
    /* need join() here */
    for(unsigned long long i = 0; i < WINDOW_SIZE; i++) {
		pthread_join(t[i], NULL);
	}
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    unsigned short port = atoi(argv[1]);

    setServerInfo(INADDR_ANY, port);
    printServerInfo(port);
    setClientInfo();
    createSocket();
    bindSocket();
    pthread_mutex_init(&mutex, NULL);
    FILE *fd;
    char command[96];
    char message[64];

    while (true) {
        memset(command, '\0', sizeof(command));
        memset(message, '\0', sizeof(message));

        printf("Server is waiting...\n");
        recvCommand(command);

        printf("Processing command...\n");
        char *str = strtok(command, " ");

        if (strcmp(str, "download") == 0) {
            str = strtok(NULL, "");
            printf("Filename is %s\n", str);

            if ((fd = getFile(str))) {
                snprintf(message, sizeof(message) - 1, "FILE_SIZE=%zu", getFileSize(fd));
                size_t filesize = getFileSize(fd);
                sendMessage(message);
                printf("══════ Sending ═══════\n");
                Ack_size = (filesize + MTU  - 1) / MTU;
                Ack_field = malloc(sizeof(bool) * Ack_size); // for checking timeout
                for(int i = 0; i < Ack_size; i++) {
                    Ack_field[i] = false;
                }
                memset(&Cwnd, 0, sizeof(Cwnd)); // initialize the window
                selective_repeat(str); // selective repeat protocol
                printf("══════════════════════\n");

                fclose(fd);
                fd = NULL;
                free(Ack_field);
                Ack_field = NULL;
            } else {
                printf("File does not exist\n");
                sendMessage("NOT_FOUND");
            }
            continue;
        }
        printf("Invalid command\n");
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}
