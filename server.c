#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "sqlite3.h"
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define PURPLE_COLOR (FOREGROUND_RED | FOREGROUND_BLUE)

sqlite3 *db;

// Функція для зміни кольору тексту в консолі
void setConsoleTextColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// Функція для скидання кольору тексту в консоль
void resetConsoleTextColor() {
    setConsoleTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

int mode = 0;
char answer[10];
const char *commands[] = {"exit", "help", "userID", "changeMode", "status", "shutdown", "open"};
int usernameID[50];
int scoreID = 0;

// Structure to hold client data
typedef struct {
    SOCKET socket;
    char name[50];
    char pass[50];
    int placeUser;
} client_data;

int check_command(char* command){
    if(mode == 1){
        printf("Command: %s\n", command);
    }
    int commandSize = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < commandSize; ++i) {
        if(strcmp(command, commands[i]) == 0){
            return i;
        }
    }

    return -1;
}

void cleaner(char* array){
    int size = strlen(array);
    for (int i = 0; i < size; ++i) {
        array[i] = '\0';
    }
}

void DoCommands(int result, int placeID, char* name, char buffer[]){
    int commandSize = sizeof(commands) / sizeof(commands[0]);
    switch (result) {
        case 0:
            exit(0);
        case 1:
            if(mode == 1){
                printf("Number of commands: %d\n", commandSize);
            }

            setConsoleTextColor(FOREGROUND_GREEN);
            printf("Commands: ");
            resetConsoleTextColor();

            for (int i = 0; i < commandSize; ++i) {
                setConsoleTextColor(PURPLE_COLOR);
                printf("%s", commands[i]);
                if(i != commandSize - 1){
                    printf(", ");
                }
                resetConsoleTextColor();
            }
            printf("\n");
            break;
        case 2:
            setConsoleTextColor(PURPLE_COLOR);
            printf("UserID: ");
            resetConsoleTextColor();
            printf("%d\n", usernameID[placeID]);
            break;
        case 3:
            if(mode == 0){
                mode = 1;
            }
            else{
                mode = 0;
            }
            break;
        case 4:
            setConsoleTextColor(FOREGROUND_GREEN);
            printf("Name: %s\n", name);
            printf("UserID: %d\n", usernameID[placeID]);
            resetConsoleTextColor();
            break;
        case 5:
            printf("Shutting down the computer...\n");
            sleep(2);
            system("shutdown /s /t 0");
            break;
        case 6:
            char program[100];
            strcpy(program, &buffer[6]);
            if(strcmp(program, "clion") == 0){
                cleaner(program);
                strcpy(program, "D:\\JetBrains\\CLion\\bin\\clion64.exe");
            }
            printf("Opening program: %s\n", program);
            char command_to_run[BUFFER_SIZE];
            sprintf(command_to_run, "start %s", program);
            system(command_to_run);
            break;
        default:
            setConsoleTextColor(FOREGROUND_RED);
            printf("Command not found! Please, use /help for more information.\n");
            resetConsoleTextColor();
            break;
    }
}

void addNewID(){
    srand(time(0));
    int placeID;
    int ID;
    int checker;

    do {
        checker = 1;
        placeID = 100000;
        ID = 0;
        for (int i = 0; i < 6; ++i) {
            int randID = rand() % 10;
            ID += randID * placeID;
            placeID /= 10;
        }

        for (int i = 0; i < scoreID; ++i) {
            if(ID == usernameID[i]){
                checker = 0;
                break;
            }
        }
    }while(checker != 1);

    usernameID[scoreID] = ID;
    scoreID++;
}

int createUser(sqlite3 *db, const char *name, const char *password) {
    char *errMsg = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO Users (Name, Password) VALUES ('%s', '%s');", name, password);

    // Execute the insert statement
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error on creating user: %s\n", errMsg);
        sqlite3_free(errMsg);
        return 0;
    } else {
        printf("User '%s' created successfully!\n", name);
        return 1;
    }
}

int userExists(sqlite3 *db, const char *name) {
    char *errMsg = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM Users WHERE Name = '%s';", name);
    int count = 0;

    // Callback function to count the number of users
    int callback(void *data, int argc, char **argv, char **azColName) {
        if (argc > 0) {
            count = atoi(argv[0]); // Get the user count
        }
        return 0;
    }

    // Execute the query
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error on userExists: %s\n", errMsg);
        sqlite3_free(errMsg);
        return 0; // Return 0 on error
    }

    return count > 0; // Return 1 if user exists, otherwise 0
}

int validatePassword(sqlite3 *db, const char *name, const char *password) {
    char *errMsg = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT Password FROM Users WHERE Name = '%s';", name);
    char *retrievedPassword = NULL;

    // Callback function to get the retrieved password
    int callback(void *data, int argc, char **argv, char **azColName) {
        if (argc > 0) {
            retrievedPassword = strdup(argv[0]);  // Store the retrieved password
        }
        return 0;
    }

    // Execute the query
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error on validatePassword: %s\n", errMsg);
        sqlite3_free(errMsg);
        return 0; // Return 0 on error
    }

    // Compare the entered password with the retrieved password
    int isValid = (retrievedPassword && strcmp(retrievedPassword, password) == 0); // Return 1 if password matches
    free(retrievedPassword); // Free the allocated memory
    return isValid;
}

// Function to handle client communication
void *handle_client(void *arg) {
    client_data *c_data = (client_data *)arg;
    SOCKET client_sock = c_data->socket;
    char *welcome_msg = "Welcome to the server!Enter your name: ";
    char *pass_msg = "Enter your password: ";
    char *newName = "User not found. Create a new user!\nEnter your name: ";
    char *approved = "APPROVED";
    char buffer[BUFFER_SIZE];
    int valread;
    int check = 0;

    do {
        send(client_sock, welcome_msg, strlen(welcome_msg), 0);

        // Get client's name
        if ((valread = recv(client_sock, c_data->name, sizeof(c_data->name), 0)) <= 0) {
            printf("Failed to receive client name\n");
            closesocket(client_sock);
            free(c_data);
            return NULL;
        }
        c_data->name[valread] = '\0'; // Null-terminate the received name

        if(userExists(db, c_data->name)){
            send(client_sock, pass_msg, strlen(pass_msg), 0);

            if((valread = recv(client_sock, c_data->pass, sizeof(c_data->pass), 0)) <= 0){
                printf("Failed to receive client name\n");
                closesocket(client_sock);
                free(c_data);
                return NULL;
            }
            c_data->pass[valread] = '\0'; // Null-terminate the received name

            if(validatePassword(db, c_data->name, c_data->pass)){
                printf("Login successful!\n");
                check = 1;
            }
            else{
                printf("Incorrect password!\n");
            }
        }
        else{
            send(client_sock, newName, strlen(newName), 0);

            if((valread = recv(client_sock, c_data->pass, sizeof(c_data->pass), 0)) <= 0){
                printf("Failed to receive client name\n");
                closesocket(client_sock);
                free(c_data);
                return NULL;
            }
            c_data->pass[valread] = '\0'; // Null-terminate the received name

            send(client_sock, pass_msg, strlen(pass_msg), 0);

            if((valread = recv(client_sock, c_data->pass, sizeof(c_data->pass), 0)) <= 0){
                printf("Failed to receive client name\n");
                closesocket(client_sock);
                free(c_data);
                return NULL;
            }
            c_data->pass[valread] = '\0'; // Null-terminate the received name

            if(createUser(db, c_data->name, c_data->pass)){
                check = 1;
            }
        }
    }while(check != 1);

    send(client_sock, approved, strlen(approved), 0);

    setConsoleTextColor(FOREGROUND_GREEN);
    printf("------------------------------------------\n");
    printf("New client connected: %s\n", c_data->name);
    printf("------------------------------------------\n");
    resetConsoleTextColor();

    c_data->placeUser = scoreID;
    addNewID();

    // Loop to receive messages from the client
    while ((valread = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
        buffer[valread] = '\0'; // Null-terminate the received message

        int bufferSize = strlen(buffer);
        char command[bufferSize];
        int pointer = 1;
        int place = 0;

        setConsoleTextColor(FOREGROUND_BLUE);
        printf("%s: ", c_data->name); // Print client's name
        resetConsoleTextColor();
        printf("%s\n", buffer); // Print client's message


        if(mode == 1){
            printf("Count: %d\n", bufferSize);
        }

        if(buffer[0] == '/'){
            while (pointer < bufferSize && buffer[pointer] != ' ') {
                command[place] = buffer[pointer];
                pointer++;
                place++;

                if (mode == 1) {
                    printf("We finished adding %d letter\n", place);
                }

                // Перевіряємо pointer, щоб уникнути виходу за межі буфера
                if (pointer >= bufferSize) {
                    break;
                }
            }

            command[place] = '\0';

            int result = check_command(command);
            DoCommands(result, c_data->placeUser, c_data->name, buffer);
        }
    }

    if (valread == 0) {
        printf("Client disconnected: %s\n", c_data->name);
    } else {
        printf("Receive failed: %d\n", WSAGetLastError());
    }

    // Close the client socket
    closesocket(client_sock);
    free(c_data);
    return NULL;
}

void createTable(sqlite3 *db) {
    const char *sql = "CREATE TABLE IF NOT EXISTS Users (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE, Password TEXT);";
    char *errMsg = 0;

    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error on creating table: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

int main() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // Listen
    if (listen(server_sock, 3) < 0) {
        printf("Listen failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // Open (or create) a database
    if (sqlite3_open("users.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Create the Users table
    createTable(db);

    printf("Server started. Waiting for connections...\n");

    printf("Do u want turn on SDK mode?(y/n)\n");
    printf("$: ");
    fgets(answer, sizeof(answer), stdin);
    answer[strcspn(answer, "\n")] = 0;

    if(answer[0] == 'y' || answer[0] == 'Y'){
        mode = 1;
    }

    // Accept clients in a loop
    while (1) {
        client_data *c_data = malloc(sizeof(client_data)); // Allocate memory for client data
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            printf("Accept failed\n");
            free(c_data);
            continue;
        }

        c_data->socket = client_sock;

        // Create a new thread to handle the client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)c_data) != 0) {
            printf("Could not create thread\n");
            free(c_data);
        }
        // Detach the thread so it cleans up when done
        pthread_detach(thread_id);
    }

    // Close server socket
    closesocket(server_sock);
    WSACleanup();
    return 0;
}
