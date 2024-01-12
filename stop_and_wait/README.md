# RDT with stop-and-wait

Implement the stop-and-wait mechanism by Linux socketing programming at both the client side and the server side. 
The client should be able to download a file from the server using this mechanism over a UDP socket. 
The main objective is to have hands-on experience in implementing Automatic Repeat-request (ARQ).

# Server

- Create a UDP socket on port 7777. The server awaits requests from clients by listening to the port.
- When the server receives a download request, if the file exists, the server should respond with the file size as "FILE_SIZE={size}"; otherwise, it should reply with "NOT_FOUND".
- If the specified file exists, the server transmits that file to the client by the stop-and-wait mechanism.
- The payload size of each packet is fixed at 1024 bytes, including the last packet. The actual data of the last packet is {size} mod 1024 bytes, but padding (all zeros) should be added to the last packet such that the length of the last packet is equal to 1024 bytes.
- In case an acknowledgment (ACK) is not received within 100 milliseconds, the missing packet should be retransmitted (#define TIMEOUT 100).

# Client

- Create a UDP socket and specify the server’s IP address and port.
- Allow the user to use the command "download {filename}" to request a file from the server.
- After receiving confirmation of the file’s existence (FILE_SIZE={size}) in the response, the client can proceed to start receiving the file.
- To simulate packet loss, the client intentionally disregards each received packet with a 30% probability (#define LOSS_RATE 0.3).
- Upon successfully receiving a packet, the client should send an acknowledgment (ACK) and append the received data to the file if the associated sequence number is valid.
- Save the file with the prefix "download_" and name it as "download_{filename}".

# Demo 
![Screenshot 2024-01-12 at 10 12 36 PM](https://github.com/jeffabc1997/Socket_Programming/assets/55129114/a4312b25-3598-4744-b684-0a7137a728c6)
