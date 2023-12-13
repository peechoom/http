/**
 * 
 * @file testServer.c
 * @author ajhende4 Alec Henderson
*/

#include "http.cpp"
#include <cassert>

char testMsg[] = "GET /index.html HTTP/1.1\r\n"
               "Host: www.example.com\r\n"
               "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3\r\n"
               "\r\n"
               "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
using namespace std;
int main() {
    int pfd[2];
    pipe(pfd);
    write(pfd[1], testMsg, sizeof(testMsg));
    httpRequest req(pfd[0]);
    cout << req.method;
    cout << req.target;
    cout << req.version;
    return 0;
}
