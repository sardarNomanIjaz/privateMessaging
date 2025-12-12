#include <iostream>
#include <string>
#include <winsock2.h> 
#include <windows.h>
#include <fstream>
#include <sstream>

// removed <vector> as requested

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ========== COLORS ==========
const int WHITE = 7;
const int GREEN = 10;
const int CYAN = 11;
const int RED = 12;
const int MAGENTA = 13;
const int YELLOW = 14;

// ========== STRUCTURES ==========
struct Client {
    SOCKET socket;
    string username;
    bool isLoggedIn;
};

// ========== GLOBAL VARIABLES ==========
Client clients[100];
int clientCount = 0;

// ========== HELPER FUNCTIONS ==========

void SetColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void DrawBanner() {
    SetColor(CYAN);
    cout << "===========================================\n";
    cout << "      S U P E R   C H A T   v 2 . 0        \n";
    cout << "===========================================\n";
    SetColor(WHITE);
}

// --- FILE HANDLING: USER AUTH ---

bool RegisterUser(string user, string pass) {
    ifstream checkFile("users.txt");
    string line, u, p;
    while (getline(checkFile, line)) {
        stringstream ss(line);
        ss >> u >> p;
        if (u == user) return false; 
    }
    checkFile.close();

    ofstream outFile("users.txt", ios::app);
    outFile << user << " " << pass << endl;
    return true;
}

bool LoginUser(string user, string pass) {
    ifstream checkFile("users.txt");
    string line, u, p;
    while (getline(checkFile, line)) {
        stringstream ss(line);
        ss >> u >> p;
        if (u == user && p == pass) return true;
    }
    return false;
}

// --- FILE HANDLING: CHAT HISTORY ---

// 1. Save a message to the text file
void LogMessage(string msg) {
    ofstream historyFile("chat_history.txt", ios::app);
    if (historyFile.is_open()) {
        historyFile << msg << endl;
        historyFile.close();
    }
}

// 2. Read the text file and send lines to the new user
void SendHistory(SOCKET s) {
    ifstream historyFile("chat_history.txt");
    string line;
    while (getline(historyFile, line)) {
        // Send existing lines to the user so they can catch up
        send(s, line.c_str(), line.length(), 0);
        Sleep(10); // Small delay to prevent packet merging
    }
    historyFile.close();
}

// ========== SERVER FUNCTIONS ==========

DWORD WINAPI ClientHandler(LPVOID param) {
    long long id_long = (long long)param;
    int id = (int)id_long;

    SOCKET mySock = clients[id].socket;
    char buffer[1024];
    string currentName = "";

    // --- AUTHENTICATION PHASE ---
    while (true) {
        memset(buffer, 0, 1024);
        int bytes = recv(mySock, buffer, 1024, 0);
        if (bytes <= 0) return 0;

        string msg(buffer);
        stringstream ss(msg);
        string cmd, type, user, pass;
        ss >> cmd;

        if (cmd == "#AUTH") {
            ss >> type >> user >> pass;
            if (type == "LOGIN") {
                if (LoginUser(user, pass)) {
                    string response = "#OK";
                    send(mySock, response.c_str(), response.length(), 0);
                    
                    currentName = user;
                    clients[id].username = currentName;
                    clients[id].isLoggedIn = true;
                    cout << "User Logged In: " << currentName << endl;
                    
                    // NEW: Send them the chat history from file!
                    SendHistory(mySock);
                    
                    break;
                } else {
                    string response = "#FAIL Invalid credentials";
                    send(mySock, response.c_str(), response.length(), 0);
                }
            }
            else if (type == "SIGNUP") {
                if (RegisterUser(user, pass)) {
                    string response = "#OK_CREATED";
                    send(mySock, response.c_str(), response.length(), 0);
                    cout << "New User Registered: " << user << endl;
                } else {
                    string response = "#FAIL User already exists";
                    send(mySock, response.c_str(), response.length(), 0);
                }
            }
        }
    }

    // --- CHAT PHASE ---
    while (true) {
        memset(buffer, 0, 1024);
        int bytes = recv(mySock, buffer, 1024, 0);
        if (bytes <= 0) break;

        string msg(buffer);
        
        // Check if message starts with '/'
        if (msg.size() > 1 && msg[0] == '/') {
            stringstream ss(msg);
            string tempTarget, content;

            ss >> tempTarget; 
            string target = tempTarget.substr(1);
            getline(ss, content); 
            
            if (!content.empty() && content[0] == ' ') content = content.substr(1);

            bool found = false;
            for (int i = 0; i < clientCount; i++) {
                if (clients[i].username == target && clients[i].isLoggedIn) {
                    string privMsg = "[Private from " + currentName + "]: " + content;
                    send(clients[i].socket, privMsg.c_str(), privMsg.length(), 0);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                string errMsg = "[Server] User '" + target + "' not found or offline.";
                send(mySock, errMsg.c_str(), errMsg.length(), 0);
            }
        }
        else {
            // Public Broadcast
            string message = currentName + ": " + msg;
            cout << message << endl;

            // NEW: Log this public message to file
            LogMessage(message);

            for (int i = 0; i < clientCount; i++) {
                if (i != id && clients[i].isLoggedIn) {
                    send(clients[i].socket, message.c_str(), message.length(), 0);
                }
            }
        }
    }
    
    cout << currentName << " disconnected.\n";
    clients[id].isLoggedIn = false;
    closesocket(mySock);
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

    SetColor(GREEN);
    cout << "Server Started on Port 8888.\n";
    SetColor(WHITE);

    while (true) {
        SOCKET newSocket = accept(serverSocket, NULL, NULL);
        clients[clientCount].socket = newSocket;
        clients[clientCount].isLoggedIn = false; 
        CreateThread(NULL, 0, ClientHandler, (LPVOID)(long long)clientCount, 0, NULL);
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
            string msg = string(buffer);
            
            // COLOR LOGIC based on message content
            if (msg.find("[Private") == 0) {
                SetColor(MAGENTA);
                cout << "\r" << msg << endl; 
            } 
            else if (msg.find("[Server]") == 0) {
                SetColor(YELLOW);
                cout << "\r" << msg << endl; 
            }
            else {
                SetColor(CYAN);
                cout << "\r" << msg << endl;
            }
            
            SetColor(WHITE);
            cout << "> " ; 
        } else {
            break;
        }
    }
    return 0;
}

void RunClient() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    // --- CONNECT SCREEN ---
    system("cls"); 
    DrawBanner();
    
    string ipAddress;
    cout << "Enter Server IP (use 127.0.0.1 for local): ";
    SetColor(GREEN);
    cin >> ipAddress;
    SetColor(WHITE);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = inet_addr(ipAddress.c_str());

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        SetColor(RED);
        cout << "Could not connect! Is the server running?\n";
        SetColor(WHITE);
        return;
    }

    // --- AUTHENTICATION UI ---
    bool authenticated = false;
    while (!authenticated) {
        system("cls");
        DrawBanner();
        cout << "\n1. Login\n2. Signup\n";
        cout << "Choose option: ";
        
        int choice;
        cin >> choice;
        
        string user, pass;
        cout << "Username: "; 
        SetColor(GREEN); cin >> user; SetColor(WHITE);
        
        cout << "Password: "; 
        SetColor(GREEN); cin >> pass; SetColor(WHITE);

        string request;
        if (choice == 1) request = "#AUTH LOGIN " + user + " " + pass;
        else request = "#AUTH SIGNUP " + user + " " + pass;

        send(clientSocket, request.c_str(), request.length(), 0);

        char buffer[1024];
        memset(buffer, 0, 1024);
        recv(clientSocket, buffer, 1024, 0);
        string response(buffer);

        if (response.find("#OK") != string::npos) {
            if (choice == 1) {
                authenticated = true;
            } else {
                SetColor(GREEN);
                cout << "\nSignup Successful! Press Enter to login...";
                cin.ignore(); cin.get();
                SetColor(WHITE);
            }
        } else {
            SetColor(RED);
            cout << "\nError: " << response << "\nPress Enter to try again.";
            cin.ignore(); cin.get();
            SetColor(WHITE);
        }
    }

    // --- CHAT UI ---
    system("cls");
    DrawBanner();
    SetColor(GREEN);
    cout << "Connected as " << "\n";
    SetColor(WHITE);
    cout << "-------------------------------------------\n";
    cout << "Typing text broadcasts to everyone.\n";
    cout << "Type "; SetColor(MAGENTA); cout << "/Name Message"; SetColor(WHITE); cout << " for private chat.\n";
    cout << "-------------------------------------------\n\n";

    CreateThread(NULL, 0, Receiver, (LPVOID)clientSocket, 0, NULL);

    string msg;
    getline(cin, msg); 
    
    cout << "> ";
    while (true) {
        getline(cin, msg);
        if (msg == "exit") break;
        send(clientSocket, msg.c_str(), msg.length(), 0);
        cout << "> ";
    }
}

int main() {
    SetColor(WHITE); 
    system("cls");
    
    cout << "1. Run Server\n";
    cout << "2. Run Client\n";
    cout << "Choice: ";
    int choice;
    cin >> choice;

    if (choice == 1) RunServer();
    else RunClient();

    return 0;
}
