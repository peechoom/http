/**
 * 
 * @file server.cpp
 * @author ajhende4 Alec Henderson
*/
#include <cstring>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <filesystem>
#include <fstream>
#include <thread>
#include <csignal>


#include "http.cpp"

#define HTTP_PORT "8000"
#define LISTEN_QUEUE_LEN 8
#define BUFFER_SIZE 2048
#define BLOCK_SIZE 1024

using namespace std;

const char TERMINATE_HEADER[2] = {'\r', '\n'};
//TODO: make this work wherever
set<string> allowed;
bool running = false;
int boundSocket = -1;
const string notFound = "HTTP/3 404\r\n"
                        "Content-Length: 0";

void sendMessage(int socket, httpResponse &response) {
    string header = response.header();

    auto len = header.size();
    const char *x = header.c_str();
    cout << "sending file \"" << response.filepath << "\"\n";

    while(len > 0) {
        ssize_t bytesWritten;
        if(header.size() < BLOCK_SIZE) {
            bytesWritten = write(socket, x, header.size());
        } else {
            bytesWritten = write(socket, x, BLOCK_SIZE);
        }

        if(bytesWritten < 0) {
            cerr << "error writing header to socket\n";
            return;
        } else if(bytesWritten == 0) {
            cerr << "connection closed\n";
            return;
        }
        x += bytesWritten;
        len -= bytesWritten;
    }


    if(response.filepath.empty()) {
        return;
    }
    if(!filesystem::exists(response.filepath)) {
        cerr << "file \"" << response.filepath << "\" DNE\n";
        return;
    }

    std::ifstream inputFile(response.filepath, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening image file." << std::endl;
        return;
    }


    char data[BLOCK_SIZE];
    ssize_t bytesWritten;
    auto n = filesystem::file_size(response.filepath);

    while(n > 0 && inputFile.good()) {
        inputFile.read(data, BLOCK_SIZE);
        if(n < BLOCK_SIZE) {
            bytesWritten = write(socket, data, n);
        } else {
            bytesWritten = write(socket, data, BLOCK_SIZE);
        }
        if(bytesWritten < 0) {
            cerr << "error writing file to socket\n";
            return;
        } else if(bytesWritten == 0) {
            cerr << "connection closed\n";
            return;
        }
        n -= bytesWritten;
    }
}

void *handleClient(void *arg) {
    int sock = (int) (long) arg;
    char buffer[BUFFER_SIZE] = {};
    read(sock, buffer, BUFFER_SIZE);

    httpRequest req(buffer, BUFFER_SIZE); //TODO this should be bytes but bytes always =1
    httpResponse resp(allowed, req);

    sendMessage(sock, resp);

    close(sock);
}

void sigHandler(int) {
    cout << "shutting down\n";
    running = false;
    close(boundSocket);
    exit(0);
}

int startServer() {

    struct sigaction sig{};
    sig.sa_flags = 0;
    sigemptyset(&sig.sa_mask);
    sig.sa_handler = sigHandler;
    sigaction(SIGINT, &sig, nullptr);
    sigaction(SIGTERM, &sig, nullptr);
    sigaction(SIGKILL, &sig, nullptr);

    // Prepare a description of server address criteria.
    struct addrinfo addrCriteria{};
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_INET;
    addrCriteria.ai_flags = AI_PASSIVE;
    addrCriteria.ai_socktype = SOCK_STREAM;
    addrCriteria.ai_protocol = IPPROTO_TCP;

    struct addrinfo *servAddr;
    if(getaddrinfo(nullptr, HTTP_PORT, &addrCriteria, &servAddr)) {
        perror("cant get address info");
        exit(1);
    }

    //try and use first address
    if(servAddr == nullptr) {
        perror("can't get address");
        exit(1);
    }

    boundSocket = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);

    if(boundSocket < 0) {
        perror("cant create socket");
        exit(1);
    }

    if(bind(boundSocket, servAddr->ai_addr, servAddr->ai_addrlen) != 0) {
        perror("cant bind socket");
        exit(1);
    }
    if(listen(boundSocket, LISTEN_QUEUE_LEN) != 0) {
        perror("couldn't listen to port");
        exit(1);
    }
    struct sockaddr_in clientAddr = {};
    socklen_t clientAddrLen = sizeof (clientAddr);

    running = true;
    while(running) {
        int sock = accept(boundSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);

        pthread_t clientThread;
        pthread_create(&clientThread, nullptr, handleClient, (void *) (long) sock);
        //pthread_join(clientThread, nullptr);
        pthread_detach(clientThread);
    }
    close(boundSocket);

}

inline void addToFileset(const string& filepath) {
    static const unsigned long len = filepath.size();
    for (const auto & entry : filesystem::directory_iterator(filepath)) {
        string p = entry.path();
        cout << "adding " << p << '\n';
        if(filesystem::is_directory(p)) {
            addToFileset(p);
        } else {
            allowed.insert(p.substr(len));
        }
    }
}

int main(int argc, char *argv[]) {
    path = "/home/alec/Documents/misc programming/http/siteFiles";
    addToFileset(path);
    startServer();
}
