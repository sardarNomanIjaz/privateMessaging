#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>

using namespace std;

// ========== CONSTANTS ==========
const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 10;
const int MAX_USERS = 50;

// ========== STRUCTURES ==========
struct User {
    string username;
    string password;
};

struct Client {
    SOCKET socket;
    string username;
    int active;
};

// ========== GLOBAL ARRAYS ==========
User registeredUsers[MAX_USERS];
Client connectedClients[MAX_CLIENTS];
int userCount = 0;
int clientCount = 0;

// ========== USER FUNCTIONS ==========
int findUser(string username) {
    for (int i = 0; i < userCount; i++) {
        if (registeredUsers[i].username == username) {
            return i;
        }
    }
    return -1;
}

int findClient(string username) {
    for (int i = 0; i < clientCount; i++) {
        if (connectedClients[i].active == 1) {
            if (connectedClients[i].username == username) {
                return i;
            }
        }
    }
    return -1;
}

// ========== MESSAGING FUNCTIONS ==========
void sendPrivateMessage(string sender, string receiver, string message) {
    int receiverIndex = findClient(receiver);
    int senderIndex = findClient(sender);
    
    if (receiverIndex == -1) {
        string errorMsg = "Error: User not online.\n";
        send(connectedClients[senderIndex].socket, errorMsg.c_str(), errorMsg.length(), 0);
        return;
    }
    
    string fullMsg = "[PM from " + sender + "]: " + message + "\n";
    send(connectedClients[receiverIndex].socket, fullMsg.c_str(), fullMsg.length(), 0);
    
    string confirmMsg = "Message sent!\n";
    send(connectedClients[senderIndex].socket, confirmMsg.c_str(), confirmMsg.length(), 0);
}

void broadcastMessage(string sender, string message) {
    string fullMsg = "[" + sender + "]: " + message + "\n";
    
    for (int i = 0; i < clientCount; i++) {
        if (connectedClients[i].active == 1) {
            if (connectedClients[i].username != sender) {
                send(connectedClients[i].socket, fullMsg.c_str(), fullMsg.length(), 0);
            }
        }
    }
}

// ========== GET SYSTEM IP ADDRESS ==========
string getSystemIPAddress() {
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostName, NULL, &hints, &result) == 0) {
        struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
        char ipBuffer[INET_ADDRSTRLEN];
        const char* ipStr = inet_ntoa(addr->sin_addr);
        string ipAddress = ipStr;
        freeaddrinfo(result);
        return ipAddress;
    }
    
    return "127.0.0.1";
}

// ========== FIND FREE PORT ==========
int findFreePort() {
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    sockaddr_in testAddr;
    testAddr.sin_family = AF_INET;
    testAddr.sin_addr.s_addr = INADDR_ANY;
    testAddr.sin_port = 0;
    
    bind(testSocket, (sockaddr*)&testAddr, sizeof(testAddr));
    
    int addrLen = sizeof(testAddr);
    getsockname(testSocket, (sockaddr*)&testAddr, &addrLen);
    
    int freePort = ntohs(testAddr.sin_port);
    closesocket(testSocket);
    
    return freePort;
}

// ========== CLIENT HANDLER ==========
DWORD WINAPI handleClient(LPVOID param) {
    int clientIndex = *((int*)param);
    delete (int*)param;
    
    char buffer[BUFFER_SIZE];
    string username = connectedClients[clientIndex].username;
    
    string welcomeMsg = "Welcome! Type /help for commands.\n";
    send(connectedClients[clientIndex].socket, welcomeMsg.c_str(), welcomeMsg.length(), 0);
    
    int isRunning = 1;
    while (isRunning == 1) {
        memset(buffer, 0, BUFFER_SIZE);
        
        int bytesReceived = recv(connectedClients[clientIndex].socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytesReceived <= 0) {
            isRunning = 0;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        string message = buffer;
        
        while (message.length() > 0) {
            char lastChar = message[message.length() - 1];
            if (lastChar == ' ' || lastChar == '\n' || lastChar == '\r' || lastChar == '\t') {
                message = message.substr(0, message.length() - 1);
            } else {
                break;
            }
        }
        
        if (message.length() == 0) {
            continue;
        }
        
        if (message[0] == '/') {
            if (message == "/help") {
                string helpMsg = "Commands:\n/pm username message - Private message\n/list - Show online users\n/help - This help\n";
                send(connectedClients[clientIndex].socket, helpMsg.c_str(), helpMsg.length(), 0);
            }
            else if (message == "/list") {
                string userList = "Online users: ";
                
                for (int i = 0; i < clientCount; i++) {
                    if (connectedClients[i].active == 1) {
                        userList = userList + connectedClients[i].username + ", ";
                    }
                }
                userList = userList + "\n";
                send(connectedClients[clientIndex].socket, userList.c_str(), userList.length(), 0);
            }
            else if (message.length() > 4 && message.substr(0, 4) == "/pm ") {
                string restOfMessage = message.substr(4);
                
                int spacePosition = -1;
                for (int i = 0; i < restOfMessage.length(); i++) {
                    if (restOfMessage[i] == ' ') {
                        spacePosition = i;
                        break;
                    }
                }
                
                if (spacePosition != -1) {
                    string receiver = restOfMessage.substr(0, spacePosition);
                    string privateMsg = restOfMessage.substr(spacePosition + 1);
                    sendPrivateMessage(username, receiver, privateMsg);
                }
            }
        }
        else {
            broadcastMessage(username, message);
        }
    }
    
    connectedClients[clientIndex].active = 0;
    closesocket(connectedClients[clientIndex].socket);
    cout << username << " disconnected.\n";
    
    return 0;
}

// ========== SERVER ==========
void runServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    string serverIP = getSystemIPAddress();
    int serverPort = findFreePort();
    
    cout << "========================================\n";
    cout << "SERVER INFORMATION\n";
    cout << "========================================\n";
    cout << "IP Address: " << serverIP << "\n";
    cout << "Port: " << serverPort << "\n";
    cout << "========================================\n";
    cout << "Give this IP and Port to clients!\n";
    cout << "========================================\n\n";
    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    char reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(serverPort);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Bind failed. Cannot start server.\n";
        WSACleanup();
        return;
    }
    
    listen(serverSocket, 10);
    cout << "Server is now running and waiting for connections...\n\n";
    
    int serverRunning = 1;
    while (serverRunning == 1) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        
        char authBuffer[200];
        memset(authBuffer, 0, 200);
        recv(clientSocket, authBuffer, 199, 0);
        string authData = authBuffer;
        
        string action = "";
        string username = "";
        string password = "";
        
        int colonCount = 0;
        int startPos = 0;
        
        for (int i = 0; i <= authData.length(); i++) {
            if (i == authData.length() || authData[i] == ':') {
                string part = authData.substr(startPos, i - startPos);
                
                if (colonCount == 0) {
                    action = part;
                }
                else if (colonCount == 1) {
                    username = part;
                }
                else if (colonCount == 2) {
                    password = part;
                }
                
                colonCount = colonCount + 1;
                startPos = i + 1;
            }
        }
        
        int authSuccess = 0;
        string response = "";
        
        if (action == "REGISTER") {
            int userExists = findUser(username);
            
            if (userExists != -1) {
                response = "ERROR:Username taken\n";
            }
            else {
                registeredUsers[userCount].username = username;
                registeredUsers[userCount].password = password;
                userCount = userCount + 1;
                response = "SUCCESS:Registered!\n";
                authSuccess = 1;
            }
        }
        else if (action == "LOGIN") {
            int userIndex = findUser(username);
            
            if (userIndex == -1) {
                response = "ERROR:User not found\n";
            }
            else if (registeredUsers[userIndex].password != password) {
                response = "ERROR:Wrong password\n";
            }
            else {
                int alreadyOnline = findClient(username);
                if (alreadyOnline != -1) {
                    response = "ERROR:Already online\n";
                }
                else {
                    response = "SUCCESS:Logged in!\n";
                    authSuccess = 1;
                }
            }
        }
        
        send(clientSocket, response.c_str(), response.length(), 0);
        
        if (authSuccess == 1) {
            connectedClients[clientCount].socket = clientSocket;
            connectedClients[clientCount].username = username;
            connectedClients[clientCount].active = 1;
            
            cout << username << " connected.\n";
            
            int* pIndex = new int;
            *pIndex = clientCount;
            clientCount = clientCount + 1;
            
            HANDLE threadHandle = CreateThread(NULL, 0, handleClient, pIndex, 0, NULL);
            CloseHandle(threadHandle);
        }
        else {
            closesocket(clientSocket);
        }
    }
    
    closesocket(serverSocket);
    WSACleanup();
}

// ========== CLIENT MESSAGE RECEIVER ==========
DWORD WINAPI receiveMessagesFromServer(LPVOID param) {
    SOCKET clientSocket = *((SOCKET*)param);
    delete (SOCKET*)param;
    
    char buffer[BUFFER_SIZE];
    
    int running = 1;
    while (running == 1) {
        memset(buffer, 0, BUFFER_SIZE);
        
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytesReceived <= 0) {
            running = 0;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        cout << "\n" << buffer;
        cout << "> ";
        cout.flush();
    }
    
    return 0;
}

// ========== CLIENT ==========
void runClient() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    cout << "========================================\n";
    cout << "CLIENT CONNECTION\n";
    cout << "========================================\n";
    
    string serverIP;
    cout << "Enter server IP address: ";
    getline(cin, serverIP);
    
    string portString;
    cout << "Enter server port number: ";
    getline(cin, portString);
    
    int serverPort = 0;
    for (int i = 0; i < portString.length(); i++) {
        if (portString[i] >= '0' && portString[i] <= '9') {
            serverPort = serverPort * 10;
            serverPort = serverPort + (portString[i] - '0');
        }
    }
    
    cout << "\nConnecting to " << serverIP << ":" << serverPort << "...\n";
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());
    
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connection failed! Check if server is running.\n";
        WSACleanup();
        return;
    }
    
    cout << "Connected to server!\n\n";
    
    int authSuccess = 0;
    while (authSuccess == 0) {
        cout << "1. Register\n2. Login\nChoice: ";
        string choice;
        getline(cin, choice);
        
        string username;
        string password;
        
        cout << "Username: ";
        getline(cin, username);
        cout << "Password: ";
        getline(cin, password);
        
        string authMsg = "";
        if (choice == "1") {
            authMsg = "REGISTER:";
        }
        else {
            authMsg = "LOGIN:";
        }
        authMsg = authMsg + username + ":" + password;
        
        send(clientSocket, authMsg.c_str(), authMsg.length(), 0);
        
        char responseBuffer[100];
        memset(responseBuffer, 0, 100);
        recv(clientSocket, responseBuffer, 99, 0);
        string response = responseBuffer;
        cout << response << "\n";
        
        if (response.length() > 0) {
            if (response[0] == 'S') {
                authSuccess = 1;
            }
            else {
                closesocket(clientSocket);
                clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
            }
        }
    }
    
    cout << "\nYou can now send messages. Type /help for commands.\n";
    
    SOCKET* pSocket = new SOCKET;
    *pSocket = clientSocket;
    HANDLE receiveThreadHandle = CreateThread(NULL, 0, receiveMessagesFromServer, pSocket, 0, NULL);
    CloseHandle(receiveThreadHandle);
    
    cout << "> ";
    
    string message;
    while (getline(cin, message)) {
        if (message == "/quit") {
            break;
        }
        
        send(clientSocket, message.c_str(), message.length(), 0);
        cout << "> ";
    }
    
    closesocket(clientSocket);
    WSACleanup();
}

// ========== MAIN ==========
int main() {
    cout << "========================================\n";
    cout << "  SIMPLE MESSAGING APPLICATION\n";
    cout << "========================================\n";
    cout << "1. Run as Server\n";
    cout << "2. Run as Client\n";
    cout << "Choice: ";
    
    string choice;
    getline(cin, choice);
    
    if (choice == "1") {
        runServer();
    }
    else {
        runClient();
    }
    
    return 0;
}
