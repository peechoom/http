/**
 * 
 * @file server.c
 * @author ajhende4 Alec Henderson
*/

#include <sstream>
#include <memory>
#include <map>
#include <iostream>


#define URI_MAX 256
#define VERSION_MAX 48

#define IS_NEWLINE (buffer[i] == '\n' || buffer[i] == '\r')
#define IS_NOT_NEWLINE (buffer[i] != '\n' && buffer[i] != '\r')

using namespace std;



typedef enum methods {GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH, NIL} httpMethod;

class httpRequest {
public:
    int socket;
    httpMethod method;
    string target;
    string version;
    map<string, string> headers;

    httpRequest(char *buffer, ssize_t n) {
        stringstream ss;
        //parse header
        ssize_t i = 0;
        while(i < n && buffer[i] != ' ' && IS_NOT_NEWLINE) {
            ss << buffer[i];
            i++;
        }
        string m(ss.str());
        ss.str("");
        ss.clear();
        i++;
        while(i < n && buffer[i] != ' ' && IS_NOT_NEWLINE) {
            ss << buffer[i];
            i++;
        }
        target = string(ss.str());
        ss.str("");
        ss.clear();
        i++;
        while(i < n && buffer[i] != ' ' && IS_NOT_NEWLINE) {
            ss << buffer[i];
            i++;
        }
        version = string(ss.str());

        if(m == "GET") {
            method = GET;
        } else if(m == "POST") {
            method = POST;
        } else {
            cout << "HTTP method " << m << " requested but not supported\n";
            method = NIL;
            return;
        }


        while(i < n && buffer[i] != EOF && buffer[i] != '\0') {
            while(i < n && IS_NEWLINE) {
                i++;
            }

            string key, value;
            while(i < n && buffer[i] != ':') {
                key += buffer[i];
                i++;
            }
            i += 2;
            while(i < n && buffer[i] && IS_NOT_NEWLINE) {
                value += buffer[i];
                i++;
            }
            i++;
            if(!key.empty() && !value.empty()) {
                headers[key] = value;
            }
        }
    }
};



