#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define PORT 8080
#define BUFFER_SIZE 2048

int main() {
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in serv_addr;
    char message[BUFFER_SIZE] = {0};
    char answerT0server[25];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Invalid address\n");
        return 1;
    }

    // Connect to the server
    printf("Attempting to connect to server...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return 1;
    }

    while(strcmp(message, "APPROVED") != 0){
        // Receive welcome message from the server
        int valread = recv(sock, message, sizeof(message) - 1, 0);
        if (valread <= 0) {
            printf("Receive failed or server closed connection\n");
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        message[valread] = '\0';
        printf("%s", message);

        if(strcmp(message, "APPROVED") != 0){
            // Get client's name
//            printf("Enter your name: ");
            fgets(answerT0server, sizeof(answerT0server), stdin);
            answerT0server[strcspn(answerT0server, "\n")] = 0; // Remove newline character

            // Send client's name to the server
            if (send(sock, answerT0server, strlen(answerT0server), 0) == SOCKET_ERROR) {
                printf("Send failed\n");
                closesocket(sock);
                WSACleanup();
                return 1;
            }
        }
    }

    printf("\n");

    // Loop to send messages to the server
    while (1) {
        printf("Enter your message: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;
        if (send(sock, message, strlen(message), 0) == SOCKET_ERROR) {
            printf("Send failed\n");
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if(activity > 0 && FD_ISSET(sock, &readfds)){
            int valread = recv(sock, message, sizeof(message) - 1, 0);
            if (valread <= 0) {
                printf("Receive failed or server closed connection\n");
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            message[valread] = '\0';
            printf("%s", message);
            if(strcmp(message, "Closing...") == 0){
                closesocket(sock);
                WSACleanup();
                exit(0);
            }
        }
    }

    // Close the socket
    closesocket(sock);
    WSACleanup();

    return 0;
}
