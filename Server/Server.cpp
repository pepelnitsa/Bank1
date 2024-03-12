#define WIN32_LEAN_AND_MEAN 

#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <chrono>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <WS2tcpip.h>
using namespace std;

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

class AccountHolder {
public:
    AccountHolder(const string& lastName, const string& firstName, int creditRating)
        : lastName(lastName), firstName(firstName), creditRating(creditRating) {}

private:
    string lastName;
    string firstName;
    int creditRating;
    chrono::system_clock::time_point registrationDate;
};

class Operation {
public:
    enum class Type { DEPOSIT, WITHDRAWAL };
    enum class Status { PENDING, COMPLETED, CANCELED };

    Operation(Type type) : type(type), timestamp(chrono::system_clock::now()), status(Status::PENDING) {}

    Status getStatus() const { return status; }
    void setStatus(Status status) { this->status = status; }

private:
    Type type;
    chrono::system_clock::time_point timestamp;
    Status status;
};

class Account {
public:
    Account(const string& lastName, const string& firstName, int creditRating)
        : holder(make_shared<AccountHolder>(lastName, firstName, creditRating)), balance(0) {}

    void deposit(double amount) {
        lock_guard<mutex> lock(mtx);
        balance += amount;
    }

    void withdraw(double amount) {
        unique_lock<mutex> lock(mtx);
        if (balance < amount) {
            cv.wait_for(lock, chrono::seconds(3), [&] { return balance >= amount; });
            if (balance < amount) {
                throw runtime_error("Insufficient funds");
            }
        }
        balance -= amount;
    }

    void transfer(Account& recipient, double amount) {
        if (this == &recipient) {
            throw invalid_argument("Cannot transfer to the same account");
        }

        lock_guard<mutex> lockSender(mtx);
        lock_guard<mutex> lockRecipient(recipient.mtx);

        withdraw(amount);
        recipient.deposit(amount);
    }

    double getBalance() const {
        lock_guard<mutex> lock(mtx);
        return balance;
    }

private:
    shared_ptr<AccountHolder> holder;
    chrono::system_clock::time_point openDateTime;
    chrono::system_clock::time_point closeDateTime;
    double balance;
    vector<Operation> operations;
    mutable mutex mtx;
    condition_variable cv;
};

void handleClient(SOCKET clientSocket) {
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;

    do {
        iResult = recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            // Process received data
            string request(recvbuf, iResult);
            string response;

            // Example request format: "DEPOSIT 100.50"
            stringstream ss(request);
            string operation;
            double amount;
            ss >> operation >> amount;

            if (operation == "DEPOSIT") {
                // Handle deposit request
                // Here, you would call the deposit method on the account object
                response = "Deposit successful.";
            }
            else if (operation == "WITHDRAW") {
                // Handle withdrawal request
                // Here, you would call the withdraw method on the account object
                response = "Withdrawal successful.";
            }
            else if (operation == "BALANCE") {
                // Handle balance inquiry
                // Here, you would call the getBalance method on the account object
                double balance = 0.0; // Replace this with actual balance
                response = "Balance: " + to_string(balance);
            }
            else {
                response = "Invalid operation.";
            }

            // Send response back to client
            iResult = send(clientSocket, response.c_str(), response.length(), 0);
            if (iResult == SOCKET_ERROR) {
                cout << "send failed with error: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                return;
            }
        }
        else if (iResult == 0) {
            cout << "Connection closing..." << endl;
        }
        else {
            cout << "recv failed with error: " << WSAGetLastError() << endl;
            closesocket(clientSocket);
            return;
        }
    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(clientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        cout << "shutdown failed with error: " << WSAGetLastError() << endl;
    }
    closesocket(clientSocket);
}


int main()
{
    setlocale(0, "");
    system("title SERVER SIDE");

    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }


    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* result = NULL;
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        cout << "getaddrinfo failed with error: " << iResult << "\n";
        cout << "получение адреса и порта сервера прошло c ошибкой!\n";
        WSACleanup();
        return 2;
    }

    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << "\n";
        cout << "создание сокета прошло c ошибкой!\n";
        freeaddrinfo(result);
        WSACleanup();
        return 3;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        cout << "bind failed with error: " << WSAGetLastError() << "\n";
        cout << "внедрение сокета по IP-адресу прошло с ошибкой!\n";
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 4;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        cout << "listen failed with error: " << WSAGetLastError() << "\n";
        cout << "прослушка информации от клиента не началась. что-то пошло не так!\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 5;
    }

    SOCKET ClientSocket = INVALID_SOCKET;
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        cout << "accept failed with error: " << WSAGetLastError() << "\n";
        cout << "соединение с клиентским приложением не установлено! печаль!\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 6;
    }

    closesocket(ListenSocket);

    //////////////////////////////////////////////////////////

    do {
        char message[DEFAULT_BUFLEN];
        iResult = recv(ClientSocket, message, DEFAULT_BUFLEN, 0); // The recv function is used to read incoming data: https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recv
        message[iResult] = '\0';

        if (iResult > 0) {
            cout << "клиент пишет: " << message << "\n";

            int receivedNumber = atoi(message);
            int responseNumber = receivedNumber + 1;

            char responseMessage[DEFAULT_BUFLEN];
            sprintf(responseMessage, "%d", responseNumber);

            int iSendResult = send(ClientSocket, responseMessage, strlen(responseMessage), 0);

            if (iSendResult == SOCKET_ERROR) {
                cout << "send failed with error: " << WSAGetLastError() << "\n";
                cout << "упс, отправка (send) ответного сообщения не состоялась ((\n";
                closesocket(ClientSocket);
                WSACleanup();
                return 7;
            }
        }
        else if (iResult == 0) {
            // cout << "соединение закрывается...\n";
        }
        else {
            cout << "recv failed with error: " << WSAGetLastError() << "\n";
            cout << "упс, получение (recv) ответного сообщения не состоялось ((\n";
            closesocket(ClientSocket);
            WSACleanup();
            return 8;
        }

    } while (iResult > 0);

    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        cout << "shutdown failed with error: " << WSAGetLastError() << "\n";
        cout << "упс, разрыв соединения (shutdown) выдал ошибку ((\n";
        closesocket(ClientSocket);
        WSACleanup();
        return 9;
    }

    closesocket(ClientSocket);
    WSACleanup();
}
