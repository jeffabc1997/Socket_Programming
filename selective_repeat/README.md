# Selective Repeat
- Selective Repeat Protocol can be viewed in https://www2.tkn.tu-berlin.de/teaching/rn/animations/gbn_sr/

The code is using the template from the stop-and-wait folder. 
To implement the selective repeat mechanism, begin by defining the window size (e.g., #define WINDOW_SIZE 4). Then, in the context of this mechanism, effective synchronization between threads becomes crucial. 

# Server

The challenging aspect lies in implementing the movement of a sliding window and managing multiple ACK timers. In cases where ACKs are not received in a timely manner, retransmission is required only for the packets corresponding to specific sequence numbers.

# Client
Use a thread to receive packet and use another thread to send Ack and write into buffer.