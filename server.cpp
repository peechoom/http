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

#define HTTP_PORT "8080"
#define LISTEN_QUEUE_LEN 8
#define BUFFER_SIZE 2048
#define BLOCK_SIZE 1024

const char TERMINATE_HEADER[2] = {'\r', '\n'};
//TODO: make this work wherever
const std::string path = "/home/alec/Documents/misc programming/http/siteFiles";
set<string> allowed;
bool running = false;
int boundSocket = -1;
const string notFound = "HTTP/3 404\r\n"
                        "Content-Length: 0";

void sendMessage(int socket, const string& message, const string& filepath) {
    auto len = message.size();
    const char *x = message.c_str();
    cerr << "sending file \"" << filepath << "\"\n";

    while(len > 0) {
        ssize_t bytesWritten;
        if(message.size() < BLOCK_SIZE) {
            bytesWritten = write(socket, x, message.size());
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


    if(filepath.empty()) {
        return;
    }
    if(!filesystem::exists(filepath)) {
        cerr << "file \"" << filepath << "\" DNE\n";
        return;
    }

    std::ifstream inputFile(filepath, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening image file." << std::endl;
        return;
    }


    char data[BLOCK_SIZE];
    ssize_t bytesWritten;
    auto n = filesystem::file_size(filepath);

    //differentiate from header and body
    write(socket, TERMINATE_HEADER, 2);
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

void respond(unique_ptr<httpRequest> req) {
    string response = "HTTP/3 ", file;
    string t = req->target;

    if(req->method == GET && allowed.find(req->target) != allowed.end()) {
        response.append("200\r\nContent-Type: ");
        auto idx = t.find('.');
        if(idx == string::npos) {
            cerr << "idx in error\n";
        }


        if(t.find(".html")        != string::npos) {
            response.append("text/html");
        } else if(t.find(".css")  != string::npos) {
            response.append("text/css");
        } else if(t.find(".jpeg") != string::npos) {
            response.append("image/jpeg");
        }else if(t.find(".jpg")  != string::npos) {
            response.append("image/jpg");
        }else if(t.find(".ico")  != string::npos) {
            response.append("image/x-icon");
        } else {
            cerr << "unsupported filetype: " << t << '\n';
            sendMessage(req->socket, notFound, file);
        }


        response.append("\r\nContent-Length: ");
        if(!filesystem::exists(path + t)) {
            cerr << "file " << path << t << " does not exist\n";
            response.clear();
            response.append(notFound);
        } else {
            response.append(to_string(filesystem::file_size(path + t)).append("\r\n"));
            file = path + t;
        }
    } else {
        cerr << "File not found: " << req->target << '\n';
        response = notFound;
    }

    sendMessage(req->socket, response, file);
}

void *handleClient(void *arg) {
    int sock = (int) (long) arg;
    char buffer[BUFFER_SIZE] = {};
    read(sock, buffer, BUFFER_SIZE);
    unique_ptr<httpRequest> req(new httpRequest(buffer, BUFFER_SIZE)); //TODO this should be bytes but bytes always =1
    if(!req) {
        cerr << "couldn't allocate more memory!";
    }
    req->socket = sock;
    respond(std::move(req));
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

inline void initializeAllowedSet() {

    for (const auto & entry : filesystem::directory_iterator(path)) {
        string p = entry.path();
        string s;
        for(auto i = path.size(); i < p.size(); i++) {
            s += p[i];
        }
        allowed.insert(s);
    }
}

int main(int argc, char *argv[]) {
    initializeAllowedSet();
    startServer();
}
