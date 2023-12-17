/**
 * 
 * @file server.c
 * @author ajhende4 Alec Henderson
*/

#include <sstream>
#include <memory>
#include <map>
#include <iostream>
#include <vector>
#include <cstring>
#include <set>
#include <filesystem>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define IS_NEWLINE (buffer[i] == '\n' || buffer[i] == '\r')
#define IS_NOT_NEWLINE (buffer[i] != '\n' && buffer[i] != '\r')


typedef enum methods {GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH, NIL} httpMethod;

class httpRequest {
private:
    static inline ulong scanUntilCRLF(const char buffer[BUFFER_SIZE], size_t n = BUFFER_SIZE) {
        ulong i = 0;
        while(i < n && buffer[i] != '\r') {
            ++i;
        }
        return i;
    }
public:

    int socket = -1;
    httpMethod method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;

    httpRequest(char *buffer, ssize_t n) {
        using namespace std;
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
            short newlineCount = 0;
            while(i < n && IS_NEWLINE) {
                newlineCount++;
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
    explicit httpRequest(int socket) {
        using namespace std;
        char buffer[BUFFER_SIZE];
        read(socket, buffer, BUFFER_SIZE);

        char meth[9], reso[BUFFER_SIZE], ver[11];
        if(sscanf(buffer, "%8s %1023s %10s\r\n", meth, reso, ver) != 3) {
            cerr << "something went wrong reading the header\n" ;
        }

        if(strcmp(meth, "GET") == 0) {
            method = GET;
        } else if(strcmp(meth, "POST") == 0) {
            method = POST;
        } else {
            cout << "HTTP method " << meth << " requested but not supported\n";
            method = NIL;
            return;
        }
        version = string(ver);
        target = string(reso);


        long min = scanUntilCRLF(buffer) + 2;
        long max = min + scanUntilCRLF(buffer + min, BUFFER_SIZE - min);

        while(buffer[min] != '\r') {
            while (max != BUFFER_SIZE && buffer[min] != '\r') {
                ulong j = min;
                string key, value;
                while (j < BUFFER_SIZE && buffer[j] != ':') {
                    key += buffer[j];
                    j++;
                }
                j += 2;
                while (j < BUFFER_SIZE && buffer[j] && (buffer[j] != '\n' && buffer[j] != '\r')) {
                    value += buffer[j];
                    j++;
                }
                if (!key.empty() && !value.empty()) {
                    headers[key] = value;
                }
                min = max + 2;
                max = min + scanUntilCRLF(buffer + min, BUFFER_SIZE - min);
            }
            if(max == BUFFER_SIZE) {
                for(ulong i = min; i < BUFFER_SIZE; i++) {
                    buffer[i - min] = buffer[i];
                }
                read(socket, buffer + min, BUFFER_SIZE - min);
                min = 2;
                max = scanUntilCRLF(buffer + min, BUFFER_SIZE - min);
            }
        }
        long contentLen = 0;
        if(auto x = headers.find("Content-Length"); x != headers.end()) {
            contentLen = atol(x->second.c_str());
        }
        if(!contentLen) {
            return;
        }
        
        min += 2;
        for(ulong i = min; i < BUFFER_SIZE; i++) {
            buffer[i - min] = buffer[i];
        }
        std::vector<char> binary;
        while(contentLen > 0) {
            long m = contentLen < (BUFFER_SIZE - min) ? contentLen : BUFFER_SIZE - min;
            binary.insert(binary.end(), buffer, buffer + m);
            contentLen -= (BUFFER_SIZE - min);
            if(contentLen > 0) {
                read(socket, buffer, BUFFER_SIZE);
                min = 0;
            }
        }
        cout << "binary size: " << binary.size() << '\n';
        for(char c : binary) {
            cout << c;
        }
        cout << '\n';
    }
};

class httpResponse {
private:
    const std::string CRLF = "\r\n";
    const std::string notFound = "404 Not found";
public:
    std::string responseCode; //ex. 200 OK, 404 Not Found
    std::string version = "HTTP/3"; //for images, use HTTP/3
    std::string filepath;
    std::map<std::string, std::string> headers;
    std::string header;
    //constructor generates response
    httpResponse(const std::set<std::string>& allowedResources, const httpRequest& req, std::string resourcePath) {
        using namespace std;
        const string& t = req.target;
        if(req.method != GET || allowedResources.find(t) == allowedResources.end()) {
            responseCode = notFound;
            cerr << "client requested illegal resource " << t << '\n';
            return;
        }
        responseCode = "200 OK";
        auto idx = t.find('.');

        //TODO refactor this to make it better
        if(t.find(".html") != string::npos || t.find(".css") != string::npos) {
            headers["Content-Type:"] = "text/";
        } else if(t.find(".jpeg") != string::npos || t.find(".jpg") != string::npos ||
                  t.find(".png")  != string::npos) {
            headers["Content-Type:"] = "image/";
        } else {
            responseCode = notFound;
            cerr << "filetype " << t << " is illegal\n";
            return;
        }
        headers["Content-Type:"].append(t.substr(idx + 1));

        if(!filesystem::exists(resourcePath + t)) {
            responseCode = notFound;
            cerr << "could not find file " << resourcePath + t << '\n';
            return;
        }
        filepath = resourcePath + t;
        headers["Content-Length:"] = to_string(filesystem::file_size(filepath));


    }
    /**
     * assembles getHeader with blank lines at the end so that the file may
     * be printed immediately after
     * @return  the complete getHeader as a string
     */
    std::string getHeader() {
        if(!header.empty()) {
            return header;
        }
        using namespace std;
        stringstream ss;
        ss << version << ' ';
        ss << responseCode << CRLF;
        if(responseCode == notFound) {
            return ss.str();
        }
        for(const auto& it : headers) {
            ss << it.first << ' ';
            ss << it.second << CRLF;
        }
        ss << CRLF;
        return ss.str();
    }
};

