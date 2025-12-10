#include <iostream>
#include <string>
#include <winsock2.h> 
#include <windows.h>  

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ========== STRUCTURES ==========
struct Client {
    SOCKET socket;   
    string username; 
};

// ========== GLOBAL ARRAYS ==========
Client clients[10]; 
int clientCount = 0;

// ========== SERVER FUNCTIONS ==========

DWORD WINAPI ClientHandler(LPVOID param) {
    // 1. Cast the pointer to a "long long" first (to match size), then to int
    long long id_long = (long long)param; // <--- FIXED HERE (Step 1)
    int id = (int)id_long;                // <--- FIXED HERE (Step 2)
    
    SOCKET mySock = clients[id].socket;
    char buffer[1024];

    while (true) {
        memset(buffer, 0, 1024);
        int bytes = recv(mySock, buffer, 1024, 0);
        
        if (bytes <= 0) break;

        string message = clients[id].username + ": " + string(buffer);
        cout << message << endl; 

        for (int i = 0; i < clientCount; i++) {
            if (i != id) { 
                send(clients[i].socket, message.c_str(), message.length(), 0);
            }
        }
    }
    return 0;
}

void RunServer() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);       
    serverAddr.sin_addr.s_addr = INADDR_ANY; 
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    cout << "Server Started on Port 8888. Waiting for students...\n";

    while (true) {
        SOCKET newSocket = accept(serverSocket, NULL, NULL);

        char nameBuffer[100];
        recv(newSocket, nameBuffer, 100, 0);
        
        clients[clientCount].socket = newSocket;
        clients[clientCount].username = nameBuffer;
        
        cout << "User Connected: " << clients[clientCount].username << endl;

        // 2. Cast the int to "long long" before turning it into a pointer
        // This makes the compiler happy because long long is the same size as a pointer
        CreateThread(NULL, 0, ClientHandler, (LPVOID)(long long)clientCount, 0, NULL); // <--- FIXED HERE

        clientCount++;
    }
}

// ========== CLIENT FUNCTIONS ==========

DWORD WINAPI Receiver(LPVOID param) {
    SOCKET s = (SOCKET)param;
    char buffer[1024];
    
    while (true) {
        memset(buffer, 0, 1024);
        int bytes = recv(s, buffer, 1024, 0);
        if (bytes > 0) {
            cout << buffer << endl; 
        }
    }
    return 0;
}

void RunClient() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    string ipAddress;
    
    cout << "Enter Server IP (use 127.0.0.1 for local): ";
    cin >> ipAddress;
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = inet_addr(ipAddress.c_str()); 

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        cout << "Could not connect! Is server running?\n";
        return;
    }

    string name;
    cout << "Enter your name: ";
    cin >> name;
    send(clientSocket, name.c_str(), name.length(), 0);

    cout << "Connected! Start typing...\n";

    CreateThread(NULL, 0, Receiver, (LPVOID)clientSocket, 0, NULL);

    string msg;
    getline(cin, msg); 
    while (true) {
        getline(cin, msg);
        if (msg == "exit") break;
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
}

int main() {
    cout << "1. Run Server\n";
    cout << "2. Run Client\n";
    int choice;
    cin >> choice;

    if (choice == 1) RunServer();
    else RunClient();

    return 0;
}
