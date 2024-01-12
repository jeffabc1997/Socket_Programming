#include "lab.h"
struct pollfd fds[1];
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

void sendFile(FILE *fd) {
    Packet send, recv;
    // Set all fields in the packet to 0, including header.seq, header.ack,
    // header.size, header.isLast, and data payload fields
    memset(&send, 0, sizeof(send));
    memset(&recv, 0, sizeof(recv));

    // Determine the size of the file
    size_t filesize = getFileSize(fd);

    // Keep track of the current position in the file
    // 0 --> 1024 --> 2048 --> ...
    size_t current = 0, tmp;
    int rc;

    // Loop through the file, sending packets until the entire file has been sent
    while (current < filesize) {
        // Seek to the "current" position in the file
        fseek(fd, current, SEEK_SET);
        
        // Read 1024 bytes from the file into the data field of the packet we will send
        fread(send.data, sizeof(char), 1024, fd); // fd moves, we need to stop it if packet loss !!!

        // Check if the current position indicates that the last packet is to be sent
        if(current + 1024 >= filesize) {
            send.header.size = filesize - current;// If it is, set the packet size to the remaining bytes in the file
            send.header.isLast = true; // Set the isLast flag to true
        } else { // Otherwise
            send.header.size = 1024; // Set the packet size to 1024
            send.header.isLast = false;
        }
        
        printf("Send SEQ = %u\n", send.header.seq);
        // Send the packet to the client
        if (sendto(sockfd, &send, sizeof(send), 0, (struct sockaddr *)&clientInfo, sizeof(struct sockaddr_in)) == -1) {
            perror("sendto()");
            exit(EXIT_FAILURE);
        }
        
        // Wait for a response from the client using poll(..., TIMEOUT) with the POLLIN event
        // Alternatively, set the timeout with setsockopt(..., SO_RCVTIMEO, ...) after creating the socket
        rc = poll(fds, 1, TIMEOUT); /* use POLLIN in this assignment */
        // printf("rc: %i\n", rc);
        if (rc == 0) {
            printf("Timeout! Resend!\n");
            continue;
        } else if(rc < 0) {
            printf("POLL() error!\n");
            continue;
        }
        if (recvfrom(sockfd, &recv, sizeof(recv), 0, (struct sockaddr *)&clientInfo, (socklen_t *)&addrlen) == -1) {
            perror("recvfrom(client)");
            exit(EXIT_FAILURE);
        }
        printf("Received ACK = %u\n", recv.header.ack);

        // Update the current position in the file
        current += 1024;
        // Update the sequence number
        send.header.seq++;
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
    memset(fds, 0 , sizeof(fds));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
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
                sendMessage(message);

                printf("══════ Sending ═══════\n");
                sendFile(fd);
                printf("══════════════════════\n");

                fclose(fd);
                fd = NULL;
            } else {
                printf("File does not exist\n");
                sendMessage("NOT_FOUND");
            }
            continue;
        }
        printf("Invalid command\n");
    }
    return 0;
}
