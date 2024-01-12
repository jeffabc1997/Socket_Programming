#include "lab.h"

#define TRHEAD_SIZE 1
int Recv_base;
bool* Recv_field;
char* buffer;
Queue Recv_buf;
pthread_mutex_t mutex;
size_t filesize;

void writeFile(char *buffer, unsigned int, char *filename);
void enterServerInfo(char *serverIP, unsigned short *serverPort) {
    printf("═══ Enter Server Info ═══\n");
    printf("Server IP: ");
    scanf("%s", serverIP);
    printf("Server port: ");
    scanf("%hu", serverPort);
    printf("═════════════════════════\n");
}

void sendRequest(char *command, char *filename) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));

    // download<space>filename, without null terminator
    packet.header.size = strlen(command) + 1 + strlen(filename);
    packet.header.isLast = true;
    // Concatenate the command "download" with the filename
    snprintf((char *)packet.data, sizeof(packet.data) - 1, "%s %s", command, filename);

    if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serverInfo, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }
}

void recvResponse(char *response) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));

    if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serverInfo, (socklen_t *)&addrlen) == -1) {
        perror("recvfrom()");
        exit(EXIT_FAILURE);
    }
    // Copy the packet data into the response buffer
    strncpy(response, (char *)packet.data, packet.header.size);
}


void sendAck(unsigned int ack) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));

    packet.header.ack = ack;
    packet.header.isLast = true;

    if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serverInfo, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }
}

bool isLoss(double p) {
    // Generate a random number between 0 and 1
    double r = (double)rand() / RAND_MAX;
    return r < p;
}

void recvFile(char *buffer) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));
    // Keep track of the current sequence number
    unsigned int seq = 0;

    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    int rc = 0;
    int count = (filesize + MTU - 1) / MTU; // packet numbers we should receive
    while (1) {
        
        if(Recv_base >= count) { // check exit condition  
            break;
        }
        rc = poll(fds, 1, 200); /* wait too long means the server finishes */
        // printf("rc: %i\n", rc);
        if (rc == 0) {
            printf("Timeout! Check Recv_base!\n");
            continue;
        } else if(rc < 0) {
            printf("POLL() error!\n");
            continue;
        } else {
            if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serverInfo, (socklen_t *)&addrlen) == -1) {
                perror("recvfrom()");
                exit(EXIT_FAILURE);
            }
            // Simulate packet loss
            if (isLoss(LOSS_RATE)) {
                printf("Oops! Packet loss!:%i\n", packet.header.seq);
                continue;
            }
            printf("Received SEQ = %u\n", packet.header.seq);
            pthread_mutex_lock(&mutex);
            
            Push(&Recv_buf, packet);
                
            pthread_mutex_unlock(&mutex);
        }
    }
    
}
void* ack_write_mem(void* param) {
    int tid = *((int*) param);
    int current = 0;
    unsigned int seq = 0;
    Packet packet;

    memset(&packet, 0, sizeof(packet));
    int count = (filesize + MTU - 1) / MTU;
    
    while(1) {
        // pthread_mutex_lock(&m_base);
        if(Recv_base >= count) {
            // pthread_mutex_unlock(&m_base);
            break;
        }
        // pthread_mutex_unlock(&m_base);
        // wait for an available packet
        pthread_mutex_lock(&mutex);
        if(isEmpty(&Recv_buf)){
            pthread_mutex_unlock(&mutex);
            // printf("isempty, %i\n", tid);
            continue;
        } else {
            Pop(&Recv_buf);
            packet = Recv_buf.data[Recv_buf.front];
            // printf("Not empty, %i\n", tid);
            pthread_mutex_unlock(&mutex);
        }
        seq = packet.header.seq;
        // Send an acknowledgement for the received packet
        // pthread_mutex_lock(&m_base);
        if(Recv_base - WINDOW_SIZE <= seq && seq <= Recv_base - 1) { // [rcvbase‐N, rcvbase‐1]
            // pthread_mutex_unlock(&m_base);
            sendAck(seq);  
        } else if(Recv_base <= seq && seq <= Recv_base + WINDOW_SIZE - 1) { // [rcvbase, rcvbase+N‐1]
            sendAck(seq);
            
            Recv_field[seq] = true;
            if(seq == Recv_base) {
                while(Recv_field[Recv_base] == true) { // advance the recv_base
                    Recv_base++;
                }
            }
            // pthread_mutex_unlock(&m_base);
            // Copy the packet data into the buffer
            current = seq * MTU; // find the memory position
            memcpy(&buffer[current], &packet.data, packet.header.size);
            printf("recv_base: %i, tid = %i\n", Recv_base, tid);
        } else {
            // pthread_mutex_unlock(&m_base);
        }
        
    }
    pthread_exit(NULL);
}
void multi_threads() {
    time_t start, end;
    start = time(NULL);
    
    int param[TRHEAD_SIZE]; // no use
    pthread_t t[TRHEAD_SIZE];
    pthread_mutex_init(&mutex, NULL);
    
    memset(&Recv_buf, 0, sizeof(Recv_buf));
    Recv_buf.front = -1;
    Recv_buf.rear = -1;
    for(int i = 0; i < TRHEAD_SIZE; i++) {
        param[i] = i;
        pthread_create(&t[i], NULL, ack_write_mem, (void*) &param[i]); 
    }
    recvFile(buffer);
    /* need join() here */
    for(unsigned long long i = 0; i < TRHEAD_SIZE; i++) {
		pthread_join(t[i], NULL);
	}

    pthread_mutex_destroy(&mutex);
    
    end = time(NULL);
    printf("Elapsed: %ld sec\n", end - start);
}

int main() {
    // Seed the random number generator for packet loss simulation
    srand(time(NULL));
    // xxx.xxx.xxx.xxx + null terminator
    char serverIP[16] = {'\0'};
    unsigned short serverPort;

    enterServerInfo(serverIP, &serverPort);
    setServerInfo(inet_addr(serverIP), serverPort);
    createSocket();

    char command[32];
    char filename[64];
    char response[64];
    

    while (true) {
        memset(command, '\0', sizeof(command));
        memset(filename, '\0', sizeof(filename));
        memset(response, '\0', sizeof(response));
        filesize = 0;

        printf("Please enter a command:\n");
        if (scanf("%s", command) == EOF)
            break;

        if (strcmp(command, "exit") == 0)
            break;

        if (strcmp(command, "download") == 0) {
            scanf("%s", filename);
            // Send the download request
            sendRequest(command, filename);
            // Receive the response
            recvResponse(response);
            // Determine whether the file exists
            if (strcmp(response, "NOT_FOUND") == 0) {
                printf("File does not exist\n");
                continue;
            }
            // Ignore characters before "=", then read the file size
            sscanf(response, "%*[^=]=%zu", &filesize);
            printf("File size is %zu bytes\n", filesize);
            // Allocate a buffer with the file size
            buffer = malloc(filesize);
            
            printf("═══════ Receiving ═══════\n");
            int packet_total =  filesize + MTU - 1 / MTU;
            Recv_base = 0;
            Recv_field = malloc(sizeof(bool) * packet_total);
            memset(Recv_field, 0, packet_total);
            // recvFile(buffer, filesize);
            multi_threads();
            printf("═════════════════════════\n");
            writeFile(buffer, filesize, filename);
            printf("═════════════════════════\n");
            free(Recv_field);
            free(buffer);
            Recv_field = NULL;
            buffer = NULL;
            continue;
        }
        printf("Invalid command\n");
        // Clear the input buffer following the invalid command
        while (getchar() != '\n')
            continue;
    }
    close(sockfd);
    return 0;
}

void writeFile(char *buffer, unsigned int filesize, char *filename) {
    char newFilename[strlen("download_") + 64];  // filename[64]
    memset(newFilename, '\0', sizeof(newFilename));
    // Concatenate "download_" with the filename
    snprintf(newFilename, sizeof(newFilename) - 1, "download_%s", basename(filename));
    printf("Saving %s\n", newFilename);

    // Create a file descriptor 
    // Name the file as newFilename and open it in write-binary mode
    FILE* fd = fopen(newFilename, "wb");
    // Write the buffer into the file
    fwrite(buffer, sizeof(char), filesize, fd); /* sizeof = 1 */
    // Close the file descriptor
    fclose(fd);
    // Set the file descriptor to NULL
    fd = NULL;
    printf("File has been written\n");
}