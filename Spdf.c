#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h> // For open()
#include <sys/stat.h> // For mode constants

#define SERVER_PORT 9089
#define BUFFER_SIZE 1024
#define SPDF_DIR "/home/koradiyd/spdf"

// Function declaration
void handle_client(int client_socket);
void remove_file(const char *full_path);
void upload_file(int client_socket, const char *file_path, const char *file_name);
void create_path(const char *path);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("SPDF Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        // Accept incoming connection infinite times
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");

        // Handle client request
        handle_client(client_socket);

        // Close client connection
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

// Function to handle smain requests
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char command_type[10];
    char path[512];
    char filename[256];
    ssize_t bytes_received;
    int command_type_len, filename_len, path_len;
    size_t file_size;
    int file_fd;

    while (1) {
        // Receive command type
        if (recv(client_socket, &command_type_len, sizeof(int), 0) <= 0 ||    
            recv(client_socket, command_type, command_type_len, 0) <= 0) {
            perror("Failed to receive command type");
            break;
        }
       
        command_type[command_type_len] = '\0';
        printf("Received command type: %s\n", command_type);

        // Receive filename length and filename
        if (recv(client_socket, &filename_len, sizeof(int), 0) <= 0 ||
            recv(client_socket, filename, filename_len, 0) <= 0) {
            perror("Failed to receive filename");
            break;
        }
        filename[filename_len] = '\0';
        printf("Received filename: %s\n", filename);

        // Receive path length and path
        if (recv(client_socket, &path_len, sizeof(int), 0) <= 0 ||
            recv(client_socket, path, path_len, 0) <= 0) {
            perror("Failed to receive path");
            break;
        }
        path[path_len] = '\0';
        printf("Received path: %s\n", path);

        // Handle based on command type
        if (strcmp(command_type, "rmfile") == 0) {
            if (strstr(filename, ".pdf") != NULL) {
                // Replace 'smain' with 'spdf' in the path
                char updated_path[512];
                strncpy(updated_path, path, sizeof(updated_path) - 1);
                updated_path[sizeof(updated_path) - 1] = '\0'; // Null-terminate the string

                // Find and replace 'smain' with 'stext'
                char *replace_pos = strstr(updated_path, "smain");
                if (replace_pos != NULL) {
                    size_t prefix_len = replace_pos - updated_path;
                    size_t suffix_len = strlen(replace_pos + 5); // 5 is the length of 'smain'
                    
                    // Create the new path with 'stext'
                    char new_path[512];
                    snprintf(new_path, sizeof(new_path), "%.*sspdf/%s", (int)prefix_len, updated_path, replace_pos + 5);

                    // Construct the full path using the updated path
                    char full_path[512];
                    snprintf(full_path, sizeof(full_path), "/home/koradiyd/%s/%s", new_path, filename);
                    printf("Full path to delete: %s\n", full_path); // Debug output
                    remove_file(full_path);
                }
            } else {
                printf("Unsupported file type or command. Only .pdf files are handled.\n");
            }
        } else if (strcmp(command_type, "ufile") == 0) {
            // Receive file size
            if (recv(client_socket, &file_size, sizeof(size_t), 0) <= 0) {
                perror("Failed to receive file size");
                break;
            }

            // Create the full file path
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s/%s", SPDF_DIR, path, filename);

            // Create directories if they don't exist
            char *last_slash = strrchr(full_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';  // Temporarily terminate the path at the last slash
                create_path(full_path);
                *last_slash = '/';  // Restore the last slash
            }

            // Open file for writing
            file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_fd == -1) {
                perror("Failed to open file for writing");
                break;
            }

            // Receive file data
            while (file_size > 0) {
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    perror("Failed to receive file data");
                    close(file_fd);
                    break;
                }
                write(file_fd, buffer, bytes_received);
                file_size -= bytes_received;
            }

            close(file_fd);
            printf("File '%s' saved to '%s'\n", filename, full_path);
        } else {
            printf("Unknown command type: %s\n", command_type);
        }
    }

    close(client_socket);
}

void remove_file(const char *full_path) {
    char command[512];
    snprintf(command, sizeof(command), "rm -f '%s'", full_path);

    // Print command for debugging
    printf("Executing command: %s\n", command);

    // Execute the rm command to remove the file
    int ret = system(command);
    if (ret == -1) {
        perror("System call failed");
    } else {
        printf("Remove file command executed with return code %d\n", ret);
    }
}

void upload_file(int client_socket, const char *file_path, const char *file_name) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "/home/koradiyd/spdf/%s/%s", file_path, file_name);  //Forming the path

    // Create the directory if it doesn't exist
    char create_dir_cmd[BUFFER_SIZE];
    // Forming path to create Dir from command
    snprintf(create_dir_cmd, sizeof(create_dir_cmd), "mkdir -p '/home/koradiyd/spdf/%s'", file_path); 
    system(create_dir_cmd);

    // Open the file for writing
    FILE *file = fopen(full_path, "wb");
    if (file == NULL) {
        perror("File open failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    // Receive the file contents from the client and write to the file
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received == -1) {
        perror("Receive failed");
    }

    fclose(file);
    printf("File uploaded and saved as: %s\n", full_path);
}

void create_path(const char *path) {
    char temp_path[512];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    // Create directories up to the last one
    for (char *p = temp_path; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp_path, 0755);
            *p = '/';
        }
    }
    mkdir(temp_path, 0755); // Create the final directory
}
