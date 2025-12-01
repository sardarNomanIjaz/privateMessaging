#include <iostream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 5555;
const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 50;
const int MAX_GROUPS = 20;
const int MAX_GROUP_MEMBERS = 50;

// Group structure
struct Group {
    char name[100];
    char members[MAX_GROUP_MEMBERS][100];
    int memberCount;
    int active;
};

// Client structure
struct Client {
    SOCKET socket;
    char username[100];
    bool active;
};

// Global arrays
Client connectedClients[MAX_CLIENTS];
Group allGroups[MAX_GROUPS];
int clientCount = 0;
int groupCount = 0;

// Simple string functions
int stringLength(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void stringCopy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int stringCompare(const char* str1, const char* str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) return 0;
        i++;
    }
    return (str1[i] == '\0' && str2[i] == '\0') ? 1 : 0;
}

int stringStartsWith(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

void trimString(char* str) {
    int start = 0;
    int end = stringLength(str) - 1;

    while (str[start] == ' ' || str[start] == '\n' || str[start] == '\r' || str[start] == '\t') {
        start++;
    }
    while (end >= start && (str[end] == ' ' || str[end] == '\n' || str[end] == '\r' || str[end] == '\t')) {
        end--;
    }

    if (start > 0) {
        int i = 0;
        while (i <= end - start) {
            str[i] = str[start + i];
            i++;
        }
        str[i] = '\0';
    }
    else {
        str[end - start + 1] = '\0';
    }
}

int findClient(const char* username) {
    for (int i = 0; i < clientCount; i++) {
        if (connectedClients[i].active && stringCompare(connectedClients[i].username, username)) {
            return i;
        }
    }
    return -1;
}

void sendPrivateMessage(const char* sender, const char* receiver, const char* message) {
    int receiverIdx = findClient(receiver);
    int senderIdx = findClient(sender);

    if (receiverIdx == -1) {
        char errorMsg[] = "Error: User not found or offline.\n";
        send(connectedClients[senderIdx].socket, errorMsg, stringLength(errorMsg), 0);
        return;
    }

    char fullMsg[BUFFER_SIZE];
    strcpy_s(fullMsg, BUFFER_SIZE, "[PRIVATE from ");
    strcat_s(fullMsg, BUFFER_SIZE, sender);
    strcat_s(fullMsg, BUFFER_SIZE, "]: ");
    strcat_s(fullMsg, BUFFER_SIZE, message);
    strcat_s(fullMsg, BUFFER_SIZE, "\n");

    send(connectedClients[receiverIdx].socket, fullMsg, stringLength(fullMsg), 0);

    char confirmMsg[] = "Message sent.\n";
    send(connectedClients[senderIdx].socket, confirmMsg, stringLength(confirmMsg), 0);
}

void handleClient(int clientIdx) {
    char buffer[BUFFER_SIZE];
    char username[100];
    stringCopy(username, connectedClients[clientIdx].username);

    char welcomeMsg[] = "Welcome! Type /help for commands.\n";
    send(connectedClients[clientIdx].socket, welcomeMsg, stringLength(welcomeMsg), 0);

    while (connectedClients[clientIdx].active) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(connectedClients[clientIdx].socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived <= 0) break;

        buffer[bytesReceived] = '\0';
        trimString(buffer);

        if (buffer[0] == '\0') continue;

        if (buffer[0] == '/') {
            if (stringStartsWith(buffer, "/private")) {
                // Find first space after /private
                int spaceIdx = 0;
                while (buffer[spaceIdx] != ' ' && buffer[spaceIdx] != '\0') spaceIdx++;

                if (buffer[spaceIdx] == ' ') {
                    // Find receiver name
                    int recvStart = spaceIdx + 1;
                    int recvEnd = recvStart;
                    while (buffer[recvEnd] != ' ' && buffer[recvEnd] != '\0') recvEnd++;

                    if (buffer[recvEnd] == ' ') {
                        char receiver[100];
                        int k = 0;
                        for (int i = recvStart; i < recvEnd; i++) {
                            receiver[k] = buffer[i];
                            k++;
                        }
                        receiver[k] = '\0';

                        char message[BUFFER_SIZE];
                        stringCopy(message, &buffer[recvEnd + 1]);

                        sendPrivateMessage(username, receiver, message);
                    }
                }
            }
            else if (stringStartsWith(buffer, "/list")) {
                char userList[BUFFER_SIZE];
                strcpy_s(userList, BUFFER_SIZE, "Online users: ");

                for (int i = 0; i < clientCount; i++) {
                    if (connectedClients[i].active) {
                        strcat_s(userList, BUFFER_SIZE, connectedClients[i].username);
                        strcat_s(userList, BUFFER_SIZE, ", ");
                    }
                }
                strcat_s(userList, BUFFER_SIZE, "\n");
                send(connectedClients[clientIdx].socket, userList, stringLength(userList), 0);
            }
            else if (stringStartsWith(buffer, "/help")) {
                char helpMsg[] = "=== COMMANDS ===\n/private username message - Send private message\n/list - List online users\n/help - Show this help\n";
                send(connectedClients[clientIdx].socket, helpMsg, stringLength(helpMsg), 0);
            }
        }
    }

    connectedClients[clientIdx].active = 0;
    closesocket(connectedClients[clientIdx].socket);
    cout << username << " disconnected.\n";
}

void runServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return;
    }

    // Allow socket reuse
    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed. Port " << PORT << " may already be in use.\n";
        cerr << "Wait a moment and try again, or change the PORT constant.\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    listen(serverSocket, SOMAXCONN);
    cout << "Server running on port " << PORT << ". Waiting for connections...\n";

    while (1) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) continue;

        if (clientCount >= MAX_CLIENTS) {
            char fullMsg[] = "Server full\n";
            send(clientSocket, fullMsg, stringLength(fullMsg), 0);
            closesocket(clientSocket);
            continue;
        }

        char usernameBuffer[100];
        memset(usernameBuffer, 0, 100);
        int received = recv(clientSocket, usernameBuffer, 99, 0);

        if (received <= 0) {
            closesocket(clientSocket);
            continue;
        }

        usernameBuffer[received] = '\0';
        trimString(usernameBuffer);

        if (stringLength(usernameBuffer) == 0) {
            closesocket(clientSocket);
            continue;
        }

        // Check if username exists
        if (findClient(usernameBuffer) != -1) {
            char errorMsg[] = "ERROR: Username already taken!\n";
            send(clientSocket, errorMsg, stringLength(errorMsg), 0);
            closesocket(clientSocket);
            continue;
        }

        connectedClients[clientCount].socket = clientSocket;
        stringCopy(connectedClients[clientCount].username, usernameBuffer);
        connectedClients[clientCount].active = 1;

        cout << usernameBuffer << " connected.\n";

        int idx = clientCount;
        clientCount++;

        thread clientThread(handleClient, idx);
        clientThread.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
}

void receiveMessages(SOCKET serverSocket) {
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived <= 0) break;

        cout << "\n" << buffer;
        cout << "> ";
        cout.flush();
    }
}

void runClient() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    if (InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr.s_addr) <= 0) {
        cerr << "Invalid address\n";
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connection failed. Is the server running?\n";
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    char username[100];
    int loginSuccess = 0;

    while (!loginSuccess) {
        cout << "Enter your username: ";
        cin.getline(username, 100);

        trimString(username);

        if (stringLength(username) == 0) {
            cout << "Username cannot be empty!\n";
            continue;
        }

        send(clientSocket, username, stringLength(username), 0);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            cout << buffer;

            if (buffer[0] != 'E') {  // Not an ERROR
                loginSuccess = 1;
            }
            else {
                closesocket(clientSocket);
                clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                    cerr << "Connection failed.\n";
                    closesocket(clientSocket);
                    WSACleanup();
                    return;
                }
            }
        }
    }

    thread receiveThread(receiveMessages, clientSocket);
    receiveThread.detach();

    char message[BUFFER_SIZE];
    cout << "> ";
    while (cin.getline(message, BUFFER_SIZE)) {
        if (stringCompare(message, "/quit")) break;
        send(clientSocket, message, stringLength(message), 0);
        cout << "> ";
    }

    closesocket(clientSocket);
    WSACleanup();
}

int main() {
    int choice = 0;
    char input[10];

    while (choice != 1 && choice != 2) {
        cout << "1. Run as Server\n2. Run as Client\nEnter your choice (1 or 2): ";
        cin.getline(input, 10);

        if (stringLength(input) == 1 && (input[0] == '1' || input[0] == '2')) {
            choice = input[0] - '0';
        }
        else {
            cout << "Invalid choice! Please enter 1 or 2.\n\n";
        }
    }

    if (choice == 1) {
        runServer();
    }
    else {
        runClient();
    }

    return 0;
}