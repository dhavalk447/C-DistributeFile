#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SERVER_PORT 9089
#define BUFFER_SIZE 1024

//function declaration
void send_file(const char *filename, const char *destination_path, int server_socket);
void send_rmfile_command(const char *command, int server_socket);
void send_dtar_request(const char *filetype, int server_socket);
void receive_tar_file( int server_socket);
void filepath_response(int server_socket);
void filepath_send(const char *filepath, int server_socket);

int main() {
    int server_socket;
    struct sockaddr_in server_addr;
    char filename[256];
    char destination_path[256];
    char command[512];
    char filetype[256];
    // Create client socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to server
    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Smain server on port %d...\n", SERVER_PORT);

    while (1) {
        printf("Enter command (e.g., ufile, rmfile, dtar): ");
        fgets(command, sizeof(command), stdin);

        // Remove newline character from the command string
        command[strcspn(command, "\n")] = 0;

        // Parse command
        if (strncmp(command, "ufile ", 6) == 0) {
            if (sscanf(command + 6, "%255s %255s", filename, destination_path) == 2) {
                send_file(filename, destination_path, server_socket);  
            } else {
                printf("Invalid ufile command format.\n");
            }
        } else if (strncmp(command, "rmfile ", 7) == 0) {  //condition for rmfile command
            send_rmfile_command(command + 7, server_socket);
        } else if (strncmp(command, "dtar ", 5) == 0) {  //condition for dtar command
            // Handle dtar command
            char filetype[256];
            if (sscanf(command + 5, "%255s", filetype) == 1) {
                send_dtar_request(filetype, server_socket);  // Send the dtar command to the server
                receive_tar_file(server_socket);  // Handle the server's response (receive tar file)
            } else {
                printf("Invalid dtar command format.\n");
            }
        } else if (strncmp(command, "display ", 8) == 0) {
            char filepath[256];
            if (sscanf(command + 8, "%255s", destination_path) == 1) {
                filepath_send(destination_path, server_socket);
                filepath_response(server_socket);
            } else {
                printf("Invalid display command format.\n");
            }
        } else {
            printf("Unknown command.\n");
        }
    }

    close(server_socket);
    return 0;
}

// Function to send the display command and file path to the server
void filepath_send(const char *filepath, int server_socket) {
    char buffer[BUFFER_SIZE];
    const char *command_type = "display";
    
    // Print the command type and file path to be sent
    printf("Sending command type: %s\n", command_type);

    // Send command type length and command type
    int command_type_len = strlen(command_type);
    if (send(server_socket, &command_type_len, sizeof(int), 0) == -1 ||
        send(server_socket, command_type, command_type_len, 0) == -1) {
        perror("Failed to send command type");
        return;
    }
}

// Function to receive and display the response from the server
void filepath_response(int server_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(server_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Server response:\n%s\n", buffer);
    } else {
        printf("Failed to receive response or connection closed.\n");
    }
}

void send_file(const char *filename, const char *destination_path, int server_socket) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t file_size;
    size_t bytes_read;
    int filename_len = strlen(filename);
    int path_len = strlen(destination_path);
    const char *command_type = "ufile";

    // Open file for reading
    file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Calculate file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send command type length and command type
    int command_type_len = strlen(command_type);
    if (send(server_socket, &command_type_len, sizeof(int), 0) == -1 ||
        send(server_socket, command_type, command_type_len, 0) == -1) {
        perror("Failed to send command type");
        fclose(file);
        return;
    }

    // Send filename length and filename
    if (send(server_socket, &filename_len, sizeof(int), 0) == -1 ||
        send(server_socket, filename, filename_len, 0) == -1) {
        perror("Failed to send filename");
        fclose(file);
        return;
    }

    // Send destination path length and destination path
    if (send(server_socket, &path_len, sizeof(int), 0) == -1 ||
        send(server_socket, destination_path, path_len, 0) == -1) {
        perror("Failed to send destination path");
        fclose(file);
        return;
    }

    // Send file size
    if (send(server_socket, &file_size, sizeof(size_t), 0) == -1) {
        perror("Failed to send file size");
        fclose(file);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(server_socket, buffer, bytes_read, 0) == -1) {
            perror("Failed to send file data");
            fclose(file);
            return;
        }
    }

    fclose(file);
    printf("File '%s' sent successfully\n", filename);
}

void send_rmfile_command(const char *command, int server_socket) {
    char full_path[256];
    char filename[256];
    char *last_slash;
    const char *command_type = "rmfile";

    // Copy the command excluding the "rmfile " prefix
    strncpy(full_path, command, sizeof(full_path));
    full_path[sizeof(full_path) - 1] = '\0';

    // Find the last slash to extract the filename
    last_slash = strrchr(full_path, '/');
    if (last_slash == NULL) {
        printf("Invalid rmfile command format\n");
        return;
    }

    // Extract filename (the part after the last slash)
    strncpy(filename, last_slash + 1, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';

    // Null-terminate the path part and remove the filename
    *last_slash = '\0';

    // Send command type length and command type
    int command_type_len = strlen(command_type);
    if (send(server_socket, &command_type_len, sizeof(int), 0) == -1 ||
        send(server_socket, command_type, command_type_len, 0) == -1) {
        perror("Failed to send command type");
        return;
    }

    // Send filename length and filename
    int filename_len = strlen(filename);
    if (send(server_socket, &filename_len, sizeof(int), 0) == -1 ||
        send(server_socket, filename, filename_len, 0) == -1) {
        perror("Failed to send filename");
        return;
    }

    // Send path length and path
    int path_len = strlen(full_path);
    if (send(server_socket, &path_len, sizeof(int), 0) == -1 ||
        send(server_socket, full_path, path_len, 0) == -1) {
        perror("Failed to send path");
        return;
    }

    printf("rmfile command sent: filename = %s, path = %s\n", filename, full_path);
}

void send_dtar_request(const char *filetype, int server_socket) {
    char command_type[] = "dtar";
    
    // First, send the command type (e.g., "dtar")
    int command_type_len = strlen(command_type);
    if (send(server_socket, &command_type_len, sizeof(int), 0) == -1 ||
        send(server_socket, command_type, command_type_len, 0) == -1) {
        perror("Failed to send command type");
        return;
    }
    
    // Send filetype length and filetype
    int filetype_len = strlen(filetype);
    if (send(server_socket, &filetype_len, sizeof(int), 0) == -1 ||
        send(server_socket, filetype, filetype_len, 0) == -1) {
        perror("Failed to send filetype");
        return;
    }
    printf("dtar command sent: command_type = %s, type = %s\n", command_type, filetype);
    
}

void receive_tar_file(int server_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    FILE *fp;
    size_t file_size;
    size_t total_bytes_received = 0;
    char filepath[512];
    char filename[256];

    // Receive the filename
    if (recv(server_socket, filename, sizeof(filename), 0) <= 0) {
        perror("Failed to receive filename");
        return;
    }
    filename[strcspn(filename, "\n")] = '\0'; // Remove any newline

    // Ensure the sclient directory exists
    const char *dir_path = "/home/koradiyd/sclient";
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0700) != 0) {
            perror("Failed to create directory");
            return;
        }
    }

    // Construct the full file path
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

    // Open the file for writing
    fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    // Receive the file size
    if (recv(server_socket, &file_size, sizeof(file_size), 0) <= 0) {
        perror("Failed to receive file size");
        fclose(fp);
        return;
    }

    // Receive the file data
    while (total_bytes_received < file_size) {
        bytes_received = recv(server_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            perror("Failed to receive file data");
            fclose(fp);
            return;
        }
        fwrite(buffer, 1, bytes_received, fp);
        total_bytes_received += bytes_received;
    }

    fclose(fp);
    printf("Received and saved tar file as '%s'\n", filepath);
}
