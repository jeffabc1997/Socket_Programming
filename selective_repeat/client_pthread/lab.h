#include <arpa/inet.h>
#include <libgen.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

// gcc server.c -o server -pthread

#define TIMEOUT 100
#define LOSS_RATE 0.3
#define MTU 1024
#define WINDOW_SIZE 4
#define MAX_QUEUE 16
/**
 * Packet header format
 */
typedef struct header {
    unsigned int seq;
    unsigned int ack;
    unsigned int size;  // Payload size
    bool isLast;
    // Note that C does not have a built-in boolean data type,
    // the stdbool.h header is part of the C99 standard and
    // provides the data type bool, constants true and false
} Header;

/**
 * Packet structure
 */
typedef struct packet {
    Header header;
    unsigned char data[1024];
    // Use unsigned char to represent byte
    // Payload size is fixed at 1024 bytes
} Packet;

typedef struct queue {
    int front;
    int rear;
    bool flag;
    int size;
    Packet data[MAX_QUEUE];
} Queue;

/**
 * Global variables
 * This is for the sake of simplicity,
 * but keep in mind that this is not a good practice
 */
int sockfd;
struct sockaddr_in serverInfo, clientInfo;
socklen_t addrlen;

/**
 * Static inline functions
 */
static inline void setServerInfo(in_addr_t address, unsigned short port) {
    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = address;
    serverInfo.sin_port = htons(port);
    addrlen = sizeof(struct sockaddr_in);
}

static inline void setClientInfo() {
    memset(&clientInfo, 0, sizeof(clientInfo));
    clientInfo.sin_family = AF_INET;
}

static inline void createSocket() {
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }
}

static inline void bindSocket() {
    if (bind(sockfd, (struct sockaddr *)&serverInfo, sizeof(struct sockaddr_in)) == -1) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }
}


static inline int isFull(Queue* q_int){
	return (q_int->rear % MAX_QUEUE == q_int->front);
}

static inline int isEmpty(Queue* q_int){
	return (q_int->front == q_int->rear) && (q_int->flag == 0);
}

static inline void Push(Queue* q_int, Packet item){
	if (isFull(q_int) && q_int->flag == 1 || q_int->rear == MAX_QUEUE - 1 && q_int->front == -1) {
		printf("Circular Queue is full!\n");
		return;
	}
	// printf("Circular Queue add: %i\n", item.header.seq);
    q_int->rear = (q_int->rear + 1) % MAX_QUEUE;
	q_int->data[q_int->rear] = item;
    q_int->size++;
	if (q_int->front == q_int->rear) {
        q_int->flag = 1;
    }
}

static inline void Pop(Queue* q_int){
	if (isEmpty(q_int)){
		printf("Circular Queue is empty!\n");
		return;
	}
	q_int->front = (q_int->front + 1) % MAX_QUEUE;
	// printf("%i is deleted.\n", q_int->data[q_int->front].header.seq);
    q_int->size--;
	if (q_int->front == q_int->rear) q_int->flag = 0;
}

static inline void printQueue(Queue* q_int){
	if (isEmpty(q_int)){
		printf("Circular Queue is empty!\n");
		return;
	}
	printf("Circular Queue: ");
	for (int i = 0; i < MAX_QUEUE; i++)
		printf("%i ", q_int->data[i].header.seq);
	printf("\n\n");
}