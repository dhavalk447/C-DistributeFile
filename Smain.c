#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#define SERVER_PORT 9089
#define BUFFER_SIZE 1024
#define SMAN_DIR "/home/koradiyd/smain" // Change this to your Smain directory path
#define PDF_SERVER_PORT 9098 // Example port for PDF server
#define PDF_SERVER_IP "127.0.0.1" // Example IP for PDF server
#define STXT_SERVER_PORT 9120
#define STXT_SERVER_IP "127.0.0.1"  // Adjust if the Stext server is on a different machine
#define BUFFER_SIZE 1024

// Function declaration
void forward_to_stxt_server(const char *command_type, const char *filename, const char *destination_path) ;
void handle_client(int client_socket);
void prcclient(int client_socket);
void create_path(const char *path);
void forward_to_pdf_server(const char *command_type, const char *filename, const char *path);
void send_to_spdf_server(const char *command_type, const char *path, const char *filename);
void handle_dtar_request(int client_socket, const char *extension);
void send_tar_file(const char *filepath, int client_socket);
void dtar_route_to_txt(const char *extension);
void handle_display(const char *directory_path, int client_socket);
void send_filepath_client(const char *data, int client_socket);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pid_t pid;

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

    // Bind the socket
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

    printf("Smain Server listening on port %d...\n", SERVER_PORT);

    // Accept incoming connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

        // Fork a child process to handle the client
        pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            close(client_socket);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(server_socket); // Close the server socket in the child process
            handle_client(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            close(client_socket); // Close the client socket in the parent process
        }
    }

    close(server_socket);
    return 0;
}


// Function to forward .txt files to the Stext server
void forward_to_stxt_server(const char *command_type, const char *filename, const char *destination_path) {
    int pdf_socket;
    struct sockaddr_in pdf_server_addr;
    FILE *file = fopen(filename, "rb");
    size_t file_size;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    if (!file) {
        perror("Failed to open file for PDF forwarding");
        return;
    }

    // Calculate file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Create and connect to the PDF server socket
    pdf_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (pdf_socket == -1) {
        perror("Failed to create TXT server socket");
        fclose(file);
        return;
    }

    pdf_server_addr.sin_family = AF_INET;
    pdf_server_addr.sin_port = htons(STXT_SERVER_PORT);
    pdf_server_addr.sin_addr.s_addr = inet_addr(STXT_SERVER_IP);

    if (connect(pdf_socket, (struct sockaddr *)&pdf_server_addr, sizeof(pdf_server_addr)) == -1) {
        perror("Connection to TXT server failed");
        close(pdf_socket);
        fclose(file);
        return;
    }
    // Send command type length and command type
    int command_type_len = strlen(command_type);
    send(pdf_socket, &command_type_len, sizeof(int), 0);
    send(pdf_socket, command_type, command_type_len, 0);

    // Send filename length and filename
    int filename_len = strlen(filename);
    if (send(pdf_socket, &filename_len, sizeof(int), 0) == -1 ||
        send(pdf_socket, filename, filename_len, 0) == -1) {
        perror("Failed to send filename");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send destination path length and destination path
    int path_len = strlen(destination_path);
    if (send(pdf_socket, &path_len, sizeof(int), 0) == -1 ||
        send(pdf_socket, destination_path, path_len, 0) == -1) {
        perror("Failed to send destination path");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send file size
    if (send(pdf_socket, &file_size, sizeof(size_t), 0) == -1) {
        perror("Failed to send file size");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(pdf_socket, buffer, bytes_read, 0) == -1) {
            perror("Failed to send file data");
            fclose(file);
            close(pdf_socket);
            return;
        }
    }

    fclose(file);
    close(pdf_socket);
    printf("File '%s' with command type '%s' forwarded to TXT server: %s\n", filename, command_type, destination_path);
}


void handle_client(int client_socket) {
    // Call the prcclient function to process client commands
    prcclient(client_socket);
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

void forward_to_pdf_server(const char *command_type, const char *filename, const char *destination_path) {
    int pdf_socket;
    struct sockaddr_in pdf_server_addr;
    FILE *file = fopen(filename, "rb");
    size_t file_size;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    if (!file) {
        perror("Failed to open file for PDF forwarding");
        return;
    }

    // Calculate file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Create and connect to the PDF server socket
    pdf_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (pdf_socket == -1) {
        perror("Failed to create PDF server socket");
        fclose(file);
        return;
    }

    pdf_server_addr.sin_family = AF_INET;
    pdf_server_addr.sin_port = htons(PDF_SERVER_PORT);
    pdf_server_addr.sin_addr.s_addr = inet_addr(PDF_SERVER_IP);

    if (connect(pdf_socket, (struct sockaddr *)&pdf_server_addr, sizeof(pdf_server_addr)) == -1) {
        perror("Connection to PDF server failed");
        close(pdf_socket);
        fclose(file);
        return;
    }
    // Send command type length and command type
    int command_type_len = strlen(command_type);
    send(pdf_socket, &command_type_len, sizeof(int), 0);
    send(pdf_socket, command_type, command_type_len, 0);

    // Send filename length and filename
    int filename_len = strlen(filename);
    if (send(pdf_socket, &filename_len, sizeof(int), 0) == -1 ||
        send(pdf_socket, filename, filename_len, 0) == -1) {
        perror("Failed to send filename");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send destination path length and destination path
    int path_len = strlen(destination_path);
    if (send(pdf_socket, &path_len, sizeof(int), 0) == -1 ||
        send(pdf_socket, destination_path, path_len, 0) == -1) {
        perror("Failed to send destination path");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send file size
    if (send(pdf_socket, &file_size, sizeof(size_t), 0) == -1) {
        perror("Failed to send file size");
        fclose(file);
        close(pdf_socket);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(pdf_socket, buffer, bytes_read, 0) == -1) {
            perror("Failed to send file data");
            fclose(file);
            close(pdf_socket);
            return;
        }
    }

    fclose(file);
    close(pdf_socket);
    printf("File '%s' with command type '%s' forwarded to PDF server: %s\n", filename, command_type, destination_path);
}

// Function to handle client request and response
void prcclient(int client_socket) {
    char command_type[10];
    char filename[256];
    char path[256];
    int command_type_len, filename_len, path_len, extension_len;
    size_t file_size;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    char extension[BUFFER_SIZE];

    while (1) {
        // Receive command type
        if (recv(client_socket, &command_type_len, sizeof(int), 0) <= 0 ||
            recv(client_socket, command_type, command_type_len, 0) <= 0) {
            perror("Failed to receive command type");
            break;
        }
        command_type[command_type_len] = '\0';
        printf("Received command type: %s\n", command_type);

        if (strcmp(command_type, "ufile") == 0) {
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
            
            if (strstr(filename, ".pdf") != NULL) {  //condition for .pdf files
                forward_to_pdf_server(command_type, filename, path);
            } else if (strstr(filename, ".txt") != NULL) {    //condition for .txt files
                forward_to_stxt_server(command_type, filename, path);
            } else {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "/home/koradiyd/smain/%s/%s", path, filename);  //forming the path
                printf("Saving file to: %s\n", full_path);

                // Removing extra slashes
                char *last_slash = strrchr(full_path, '/');
                if (last_slash != NULL) {
                    *last_slash = '\0';
                    mkdir(full_path, 0755);
                    *last_slash = '/';
                }

                int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file_fd == -1) {
                    perror("Error opening file for writing");
                    continue;
                }

                // Receive the file size
                if (recv(client_socket, &file_size, sizeof(size_t), 0) <= 0) {
                    perror("Error receiving file size");
                    close(file_fd);
                    continue;
                }

                // Receive the file content
                while (file_size > 0) {
                    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                    if (bytes_received <= 0) {
                        perror("Error receiving file data");
                        break;
                    }
                    write(file_fd, buffer, bytes_received);
                    file_size -= bytes_received;
                }

                close(file_fd);
                printf("File '%s' saved to '%s'\n", filename, full_path);
            }
        } else if (strcmp(command_type, "rmfile") == 0) {   //Condition for rmfile command

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
            // Check if the file is a .pdf
            if (strstr(filename, ".pdf") != NULL) {  //Condition for .pdf of rmfile command
                // Send the path and filename to spdf server
                send_to_spdf_server("rmfile", path, filename);
            } else if (strstr(filename, ".txt") != NULL) {   //Condition for .txt of rmfile command
                // Send the path and filename to stxt server
                send_to_stext_server("rmfile", path, filename);
            }  else {
                // Construct the full path to the file
                char full_path[512];
                char command[512];
                
                // Fetch the home directory from the environment
                const char *home_dir = getenv("HOME");
                if (home_dir == NULL) {
                    fprintf(stderr, "Error: HOME environment variable is not set.\n");
                    return;
                }
                
                // Construct the full path based on the user's home directory
                snprintf(full_path, sizeof(full_path), "%s/%s/%s", home_dir, path, filename);

                // Prepare the rm command to remove the file
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
        } else if (strcmp(command_type, "dtar") == 0) {   // Handle the dtar command
            // Receive path length and path
            if (recv(client_socket, &filename_len, sizeof(int), 0) <= 0 ||
                recv(client_socket, filename, filename_len, 0) <= 0) {
                perror("Failed to receive extension");
                break;
            }
            filename[filename_len] = '\0'; // Ensure null-termination
            
            printf("Received file extension: %s\n", filename);
            if (strcmp(filename, ".pdf") == 0) {   //Handle .pdf of dtar command
                // Call the function to handle .pdf files
                dtar_route_to_pdf(filename);
            } else if (strcmp(filename, ".txt") == 0) {     //Handle .txt of dtar command
                dtar_route_to_txt(filename);
            } else{
                // Handle the dtar command with the received extension
                handle_dtar_request(client_socket, filename);
            }
        } else if (strcmp(command_type, "display") == 0) { 
            // Handle the display command by searching for .c files
            handle_display("/home/koradiyd/smain", client_socket);
        } else {
            printf("Unknown command type received in handle client\n");
        }
    }
}

// Function to send data to the client
void send_filepath_client(const char *data, int client_socket) {
    size_t data_length = strlen(data);
    send(client_socket, &data_length, sizeof(size_t), 0);
    send(client_socket, data, data_length, 0);
}

void handle_display(const char *directory_path, int client_socket) {
    char buffer[BUFFER_SIZE];
    FILE *fp;
    char command[512];
    char result[BUFFER_SIZE];
    size_t bytes_sent;
    int ret;

    // Create the find command to search for .c files recursively
    snprintf(command, sizeof(command), "find '%s' -type f -name '*.c'", directory_path);
    printf("Executing command: %s\n", command);  // Debugging output

    // Open a pipe to the find command
    fp = popen(command, "r");
    if (fp == NULL) {
        snprintf(buffer, sizeof(buffer), "Failed to execute find command.\n");
        perror("popen error"); // More detailed error output
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    // Read the output of the find command and send it to the client
    while (fgets(result, sizeof(result), fp) != NULL) {
        // Remove trailing newline
        result[strcspn(result, "\n")] = '\0';
        snprintf(buffer, sizeof(buffer), "%s\n", result);

        // Debugging output
        printf("Sending to client: %s\n", buffer);

        bytes_sent = send(client_socket, buffer, strlen(buffer), 0);
        if (bytes_sent == -1) {
            perror("Failed to send data to client");
            break;
        }
    }

    // Check if find command execution was successful
    if (feof(fp)) {
        snprintf(buffer, sizeof(buffer), "End of list.\n");
        send(client_socket, buffer, strlen(buffer), 0);
    } else {
        snprintf(buffer, sizeof(buffer), "Error reading find command output.\n");
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // Close the pipe
    ret = pclose(fp);
    if (ret == -1) {
        perror("pclose error");
    } else {
        printf("All .c fila paths have been sent returned\n");
    }
}

// Function to route the dtar command for .txt files to the stext server
void dtar_route_to_txt(const char *extension) {
    const char *command_type = "dtar";
    int sock;
    struct sockaddr_in server_addr;
    int extension_len, command_type_len;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(STXT_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(STXT_SERVER_IP);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send command type length and command type
    command_type_len = strlen(command_type);
    send(sock, &command_type_len, sizeof(int), 0);
    send(sock, command_type, command_type_len, 0);

    // Send command type length and command type
    extension_len = strlen(extension);
    send(sock, &extension_len, sizeof(int), 0);
    send(sock, extension, extension_len, 0);

    printf("Routed dtar command '%s' with extension '%s' to stext server\n",command_type, extension);
}


// Function to route the dtar command for .txt files to the stext server
void dtar_route_to_pdf(const char *extension) {
    // Define the command string to find .pdf files and create a tarball
    const char *spdf_dir = "/home/koradiyd/spdf";
    const char *sclient_dir = "/home/koradiyd/sclient";
    char command[256];

    // Create the find command to locate all .pdf files and tar them
    snprintf(command, sizeof(command), 
             "find %s -type f -name '*.pdf' | tar -cvf %s/pdf.tar -T -",
             spdf_dir, sclient_dir);

    // Execute the command
    int result = system(command);

    if (result == 0) {
        printf("Tar file created successfully at %s/txt.tar\n", sclient_dir);
    } else {
        printf("Error creating tar file.\n");
    }
}
// Function to route the dtar command for .pdf files to the spdf server
void handle_dtar_request(int client_socket, const char *extension) {
    char command_type[256];
    char file_extension[256];
    char tar_filename[] = "sclient_archive.tar";
    char tar_command[512];
    FILE *tar_fp;
    int bytes_received;

    // Create the tar command string
    //snprintf(tar_command, sizeof(tar_command), "find /home/koradiyd/smain -type f -name '*%s' | tar -cvf %s -T -", file_extension, tar_filename);
    snprintf(tar_command, sizeof(tar_command),
                 "find /home/koradiyd/smain -type f -name '*%s' ! -name '%s' | tar -cvf %s -T -",
                 file_extension, tar_filename, tar_filename);
    // Execute the tar command
    system(tar_command);

    // Check if the tar file was created
    if (access(tar_filename, F_OK) != -1) {
        // Send the tar filename to the client
        int filename_len = strlen(tar_filename);
        if (send(client_socket, tar_filename, filename_len, 0) == -1) {
            perror("Failed to send filename");
            return;
        }

        // Send the tar file to the client
        send_tar_file(tar_filename, client_socket);
        printf("Tar file '%s' created and sent successfully\n", tar_filename);
    } else {
        printf("Failed to create tar file '%s'\n", tar_filename);
    }
}

void send_tar_file(const char *filename, int client_socket) {
    char buffer[BUFFER_SIZE];
    FILE *fp;
    size_t bytes_read;
    struct stat file_stat;
    
    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        return;
    }

    // Get file size
    if (stat(filename, &file_stat) != 0) {
        perror("Failed to get file stats");
        fclose(fp);
        return;
    }
    size_t file_size = file_stat.st_size;

    // Send file size
    if (send(client_socket, &file_size, sizeof(file_size), 0) == -1) {
        perror("Failed to send file size");
        fclose(fp);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) == -1) {
            perror("Failed to send file data");
            fclose(fp);
            return;
        }
    }

    fclose(fp);
    printf("File '%s' sent successfully\n", filename);
}

void send_to_spdf_server(const char *command_type, const char *path, const char *filename) {
    int sock;
    struct sockaddr_in server_addr;
    int command_type_len, path_len, filename_len;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PDF_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(PDF_SERVER_IP);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send command type length and command type
    command_type_len = strlen(command_type);
    send(sock, &command_type_len, sizeof(int), 0);
    send(sock, command_type, command_type_len, 0);

    // Send filename length and filename
    filename_len = strlen(filename);
    send(sock, &filename_len, sizeof(int), 0);
    send(sock, filename, filename_len, 0);

    // Send path length and path
    path_len = strlen(path);
    send(sock, &path_len, sizeof(int), 0);
    send(sock, path, path_len, 0);

    // Close the socket
    close(sock);
}

void send_to_stext_server(const char *command_type, const char *path, const char *filename) {
    int sock;
    struct sockaddr_in server_addr;
    int command_type_len, path_len, filename_len;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(STXT_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(STXT_SERVER_IP);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send command type length and command type
    command_type_len = strlen(command_type);
    send(sock, &command_type_len, sizeof(int), 0);
    send(sock, command_type, command_type_len, 0);

    // Send filename length and filename
    filename_len = strlen(filename);
    send(sock, &filename_len, sizeof(int), 0);
    send(sock, filename, filename_len, 0);

    // Send path length and path
    path_len = strlen(path);
    send(sock, &path_len, sizeof(int), 0);
    send(sock, path, path_len, 0);

    // Close the socket
    close(sock);
}
