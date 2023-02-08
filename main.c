#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#define BACKLOG 10
#define MAX_AVAILABLE_SERVER_FILES 20

int indexClient = 0;
bool running = true;
int socket_fd;
pthread_t threads[BACKLOG];
pthread_mutex_t lock;


typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
    char message[1024];
    char *dynamicBuffer;
    char filename[256];
    int clientNo;

} pthread_arg_t;

typedef struct fileList {
    char fullPath[1024];
    int bytes;
} fileList;

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

void put_operation(int client_socket, char *message);

void list_files(const char *path, char *buffer, int *bytes_written);

void delete_operation(char *message, int new_socket_fd);

void updateFile(int new_socket_fd, char *message);

void searchFiles(char *message,int sock);
void logOperation( char *operation,  char *filename, char *searchWord);
//void handle_GET_request(int new_socket_fd, char* buffer);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

void update_fileList();

fileList *listOfFiles;

void main(int argc, char *argv[]) {

    int port, new_socket_fd;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    pthread_arg_t *pthread_arg;
    pthread_t pthread;
    socklen_t client_address_len;

    /* Get port from command line arguments or stdin. */
    port = argc > 1 ? atoi(argv[1]) : 0;
    if (!port) {
        printf("Enter Port: ");
        scanf("%d", &port);
    }

    /* Initialise IPv4 address. */
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    /* Bind address to socket. */
    if (bind(socket_fd, (struct sockaddr *) &address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }
    /* Listen on socket. */
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    /*if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }*/
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Initialise pthread attribute to create detached threads. */
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    while (1) {
        pthread_arg = (pthread_arg_t *) malloc(sizeof *pthread_arg);
        if (!pthread_arg) {
            perror("malloc");
            continue;
        }

        /* Accept connection to client. */
        client_address_len = sizeof pthread_arg->client_address;
        new_socket_fd = accept(socket_fd, (struct sockaddr *) &pthread_arg->client_address, &client_address_len);
        if (new_socket_fd == -1) {
            perror("accept");
            free(pthread_arg);
            continue;
        }

        printf("Client connected: %d\n", /*inet_ntoa(pthread_arg->client_address.sin_addr*/ indexClient);

        /* Initialise pthread argument. */
        pthread_arg->new_socket_fd = new_socket_fd;
        memset(pthread_arg->message, 0, sizeof pthread_arg->message);

        /* Create thread to serve connection to client. */

        if (pthread_create(&threads[indexClient], &pthread_attr, pthread_routine, (void *) pthread_arg) != 0) {
            perror("pthread_create");
            free(pthread_arg);
            continue;
        }

    }

}

void *pthread_routine(void *arg) {
    pthread_arg_t *pthread_arg = (pthread_arg_t *) arg;

    pthread_arg->clientNo = indexClient++;

    int new_socket_fd = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;

    int recieve = 1;

    while (recieve > 0) {

        memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
        recieve = recv(new_socket_fd, pthread_arg->message, sizeof pthread_arg->message, 0);
        int bytes_written = 0;

        if (recieve == -1) {
            perror("recv");
        } else {

            printf("Received message from client: %s\n", pthread_arg->message);
        }

        if (strncmp((pthread_arg->message), "PUT", 3) == 0) {
            put_operation(pthread_arg->new_socket_fd, pthread_arg->message);

        }else if (strncmp(pthread_arg->message, "UPDATE", 6) == 0) {
                if (pthread_arg->message[6] != ' ') {
                    memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
                    strcpy(pthread_arg->message,
                           "Wrong format of command UPDATE! (UPDATE <filename> <start_byte> <size>)");

                    send(new_socket_fd, pthread_arg->message, strlen(pthread_arg->message), 0);
                    memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
                    continue;
                }
                else{
                    updateFile(pthread_arg->new_socket_fd,pthread_arg->message);
                }


        } else if (strncmp((pthread_arg->message), "DELETE", 6) == 0) {
            char *p = pthread_arg->message;
            if (*(p + 6) != ' ') {
                memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
                send(new_socket_fd, "Wrong format! Format: DELETE <full_path+filename.extension>.\n", 62, 0);
                continue;
            }

            delete_operation(pthread_arg->message, pthread_arg->new_socket_fd);
        } else if (strncmp(pthread_arg->message, "LIST", 4) == 0) {
            bool flag = true;
            char status[50] = "DONE";

            list_files("./TEST", pthread_arg->message, &bytes_written);
            if (send(new_socket_fd, pthread_arg->message, bytes_written, 0) == -1) {
                perror("send");
                continue;
            }
        } else if (strncmp(pthread_arg->message, "GET", 3) == 0) {
            int bytesToWrite = 0;
            if (pthread_arg->message[3] == '\n') {
                strcpy(pthread_arg->message, "Wrong format of command GET! (GET <filename>)");
                send(new_socket_fd, pthread_arg->message, sizeof(pthread_arg->message), 0);
                continue;
            }

            struct stat st;

            char *filename;
            int size = strlen(pthread_arg->message + 3);
            filename = (char *) malloc(size);
            int j = 0;
            for (int i = 4; i < 256; i++) {
                if (pthread_arg->message[i] != '\n') {
                    filename[j] = pthread_arg->message[i];
                    j++;
                } else
                    break;
            }

            int fd = open(filename, O_RDONLY);

            if (fd < 0) {
                perror("Opening file.\n");
                continue;
            }

            stat(filename, &st);

            int iterations = st.st_size / sizeof(pthread_arg->message) + 2;
            bool flag = true;

            for (int i = 0; i < iterations; i++) {
                memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
                int r = read(fd, pthread_arg->message, sizeof(pthread_arg->message));

                bytesToWrite = st.st_size;
                char size[50];
                sprintf(size, "%d", bytesToWrite);
                char status[10] = "DONE";
                char var[256];
                strcpy(var, status);
                strcat(var, " ");
                strcat(var, size);
                strcat(var, "\n");

                if (r < 0) {
                    perror("Couldn't access the file!\n");
                    continue;
                }
                if (flag == true) {
                    send(new_socket_fd, var, sizeof(var) + 1, 0);
                    flag = false;
                }

                send(new_socket_fd, pthread_arg->message, sizeof(pthread_arg->message), 0);
            }

            logOperation("GET",filename,"");
            strcpy(filename, "");
            free(filename);

        } else if(strncmp(pthread_arg->message, "SEARCH", 6) == 0){
            searchFiles(pthread_arg->message, pthread_arg->new_socket_fd);
        }

        else{
            memset(pthread_arg->message, 0, sizeof(pthread_arg->message));
            strcat(pthread_arg->message, "ERROR: Unknown command\n");
            send(new_socket_fd, pthread_arg->message, sizeof("ERROR: Unknown command  "), 0);
        }
    }

    // Close socket and exit thread.
    close(new_socket_fd);
    strcpy(pthread_arg->message, "");
    strcpy(pthread_arg->filename, "");
    printf("Client %d disconnected\n", pthread_arg->clientNo);
    free(arg);
    indexClient--;

    //daca inchid serverul, acesta asteapta ca toti clientii sa se deconecteze
    if (indexClient < 0) {
        pthread_cancel(threads[pthread_arg->clientNo]);
    }
    pthread_exit(NULL);

}


void handle_GET_request(int new_socket_fd, char *buffer) {
    struct stat st;

    char *filename;
    int size = strlen(buffer + 3);
    filename = (char *) malloc(size);
    int j = 0;
    for (int i = 4; i < 256; i++) {
        if (buffer != '\n') {
            filename[j] = buffer[i];
            j++;
        } else
            break;
    }

    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        perror("Opening file.\n");
        return;
    }

    stat(filename, &st);
    int S = st.st_size;
    char *content = (char *) malloc(S);

    int r = read(fd, content, st.st_size);

    int bytesToWrite = st.st_size;
    char Size[50];
    sprintf(Size, "%d", bytesToWrite);
    char status[10] = "DONE";
    char var[256];
    strcpy(var, status);
    strcat(var, " ");
    strcat(var, Size);
    strcat(var, "\n");

    if (r < 0) {
        perror("Couldn't access the file!\n");
        return;
    }

    send(new_socket_fd, var, sizeof(var) + 1, 0);

    send(new_socket_fd, content, bytesToWrite, 0);


    strcpy(content, "");
    strcpy(filename, "");
    free(filename);
    free(content);
}

int get_file_size(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) != 0) {
        return -1;
    }
    return st.st_size;
}

void update_fileList() {
    char buffer[1024];
    int bytes_written = 0;
    list_files("./TEST", buffer, &bytes_written);

    listOfFiles = (fileList *) malloc(MAX_AVAILABLE_SERVER_FILES * sizeof(listOfFiles));

    char *path = strtok(buffer, "\n");
    int i = 0;
    while (path != NULL) {
        strcpy(listOfFiles[i].fullPath, path);
        listOfFiles[i].bytes = get_file_size(path);
        path = strtok(NULL, "\n");
        i++;
    }
    if (i == 19) {
        listOfFiles = (fileList *) realloc(listOfFiles, 2 * MAX_AVAILABLE_SERVER_FILES * sizeof(listOfFiles));
    }
}

void signal_handler(int signal_number) {

    if (signal_number == SIGTERM || signal_number == SIGINT) {
        running = false;
        printf("Received signal: %d\n", signal_number);
        // Iterate through all threads and check if they are still executing
        int i;

        for (i = 0; i < indexClient + 1; i++) {
            int thread_status;
            int status;
            //status = pthread_cancel(threads[i]);
            int join = pthread_join(threads[i], (void **) &thread_status);
        }

        close(socket_fd);
        printf("Server shutting down...\n");
        exit(0);
    }

}

void list_files(const char *path, char *buffer, int *bytes_written) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    logOperation("LIST",path,"");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // ignore the "." and ".." directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            // recursively list the files in the subdirectory
            char subdir_path[1024];
            snprintf(subdir_path, 1024, "%s/%s", path, entry->d_name);
            list_files(subdir_path, buffer, bytes_written);
        } else {
            // add the file to the buffer
            int bytes = snprintf(buffer + *bytes_written, 1024 - *bytes_written, "%s/%s\n", path, entry->d_name);
            *bytes_written += bytes;
        }
    }

    closedir(dir);
}


void put_operation(int client_socket, char *message) {
    int sock = client_socket;
    char *token = strtok(message, " ");

    if (token == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }

    token = strtok(NULL, " ");

    if (token == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }

    int name_size = atoi(token); // convertim la int numarul de octeti a numelui fisierului
    token = strtok(NULL, " ");

    if (token == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }
    char file_name[name_size + 1];
    strncpy(file_name, token, name_size);
    file_name[name_size] = '\0';

    token = strtok(NULL, " ");

    if (token == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }
    int content_size = atoi(token); // convertim la int numarul de octeti a continutului fisierului

    int fileExists = 0;
// verificam daca numele fisierului exista in lista

    update_fileList();
    for (int i = 0; i < MAX_AVAILABLE_SERVER_FILES; i++) {
        if (strcmp(file_name, listOfFiles[i].fullPath) == 0) {
            fileExists = 1;
            break;
        }
    }

    if (fileExists == 1) {
        send(sock, "Fisierul cu acest nume exista deja pe server!\n", 60, 0);
        return;
    }


    char request_message[] = "introduceti continutul\n";
    send(sock, request_message, sizeof(request_message), 0);

    char file_content[content_size];
    int bytes_received = recv(sock, file_content, content_size, 0);
    if (bytes_received > content_size) {
        memset(file_content + content_size, 0, bytes_received - content_size);
    }

    int fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0644); // deschidem fisierul pentru scriere
    int w = write(fd, file_content, content_size); // scriem continutul in fisier

    char status[50];
    if (w != content_size) {
        strcpy(status, "Err writing in file!\n");
    } else {
        strcpy(status, "DONE");
        logOperation("PUT",file_name,"");
        update_fileList();
    }

    close(fd); // inchidem fisierul

    send(sock, status, sizeof(status), 0); // trimitem statusul catre client
    //close(sock); // inchidem socket-ul
}

void delete_operation(char *message, int new_socket_fd) {
    // check if the message starts with "DELETE"
    if (strncmp(message, "DELETE", 6) == 0) {
        // check if the message is in the correct format (DELETE <filename>)
        if (message[6] == '\n') {
            strcpy(message, "Wrong format of command DELETE! (DELETE <filename>)");
            send(new_socket_fd, message, sizeof(message), 0);
            return;
        }
        // extract the filename from the message
        char *filename;
        int size = strlen(message + 7);
        filename = (char *) malloc(size);
        int j = 0;
        for (int i = 7; i < 256; i++) {
            if (message[i] != '\n') {
                filename[j] = message[i];
                j++;
            } else {
                break;
            }
        }

        // attempt to delete the file
        int status = unlink(filename);

        // check if the file was successfully deleted
        if (status == 0) {
            strcpy(message, "File successfully deleted!\n");
            logOperation("DELETE",filename,"");
        } else {
            strcpy(message, "Error deleting file!\n");
        }
        send(new_socket_fd, message, 30, 0);

        // cleanup
        strcpy(filename, "");
        free(filename);

        update_fileList();
    }
}

void updateFile(int new_socket_fd, char *message) {
    char *filename, *startByteStr, *sizeStr;
    int startByte, size;

    filename = strtok(message + 7, " ");
    if (filename == NULL){
        send(new_socket_fd,"Wrong format\n",25,0);
        return;
    }

    startByteStr = strtok(NULL, " ");
    if (startByteStr == NULL){
        send(new_socket_fd,"Wrong format\n",25,0);
        return;
    }

    sizeStr = strtok(NULL, " ");
    if (sizeStr == NULL){
        send(new_socket_fd,"Wrong format\n",25,0);
        return;
    }


    startByte = atoi(startByteStr);
    size = atoi(sizeStr);

    update_fileList();
    int fileExists=0;
    for (int i = 0; i < MAX_AVAILABLE_SERVER_FILES; i++) {
        if (strcmp(filename, listOfFiles[i].fullPath) == 0) {
            fileExists = 1;
            break;
        }
    }

    if (fileExists == 0) {
        send(new_socket_fd, "Fisierul cu acest nume nu exista pe server!\n", 60, 0);
        return;
    }

    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("Opening file.\n");
        return;
    }

    char msg[] = "Introduceti continutul:\n";
    send(new_socket_fd, msg, strlen(msg), 0);

    char newContent[size];
    int recv_size = recv(new_socket_fd, newContent, size, 0);
    if (recv_size < 0) {
        perror("Error receiving content!\n");
        return;
    }

    lseek(fd, startByte, SEEK_SET);
    int w = write(fd, newContent, size);
    if (w < 0) {
        perror("Couldn't update the file!\n");
        return;
    }

    strcpy(message, "File updated successfully!");
    send(new_socket_fd, message, strlen(message), 0);
    logOperation("UPDATE",filename,"");
}

void logOperation( char *operation,  char *filename, char *searchWord) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char dateTime[20];

    strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", t);

    pthread_mutex_lock(&lock);
    FILE *fp = fopen("./TEST/log.txt", "a");

    fprintf(fp, "%s, %s, %s", dateTime, operation, filename);

    if (searchWord) {
        fprintf(fp, ", %s", searchWord);
    }

    fprintf(fp, "\n");
    fclose(fp);
    pthread_mutex_unlock(&lock);

}

void searchFiles(char *message, int sock) {
    char *bytes;

    bytes = strtok(message + 6, " ");
    if (bytes == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }

    int size=atoi(bytes);
    char result[1024];

    char* word;
    word = strtok(NULL, " \n");
    if (word == NULL){
        send(sock,"Wrong format\n",25,0);
        return;
    }

    int foundFiles = 0;
    char foundFilesList[1024] = {0};
    update_fileList();
    int sockBytes=0;


    for (int i = 0; i < MAX_AVAILABLE_SERVER_FILES; i++) {
        if(strncmp(listOfFiles[i].fullPath,"\0\0",2)==0){
            break;
        }

        int fd = open(listOfFiles[i].fullPath, O_RDONLY);

        if (fd < 0) {
            perror("Error opening file.\n");
            return;
        }

        char fileContent[listOfFiles[i].bytes];
        int readSize = read(fd, fileContent, listOfFiles[i].bytes);

        if (readSize < 0) {
            perror("Error reading file.\n");
            send(sock,"Error reading file.\n",25,0);
            return;
        }

        if (strstr(fileContent, word) != NULL) {
            foundFiles++;
            strcat(foundFilesList, listOfFiles[i].fullPath);
            strcat(foundFilesList, " ");
            strcat(foundFilesList,"\0");
            sockBytes+=strlen(foundFilesList);
        }

        close(fd);
    }

    if (foundFiles == 0) {
        strcpy(result, "0");
    } else {
        char str [50];
        snprintf(str,10,"%d",foundFiles);
        strcpy(result,str );
        strcat(result,"\n");
        strcat(result, foundFilesList);
        strcat(result,"\n");
    }

    send(sock,result,sockBytes,0);

    logOperation("SEARCH",foundFilesList,word);
}