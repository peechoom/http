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

std::string path;

typedef enum methods {GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH, NIL} httpMethod;

class httpRequest {
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

class httpResponse {
private:
    const std::string CRLF = "\r\n";
    const std::string notFound = "404 Not found";
public:
    std::string responseCode; //ex. 200 OK, 404 Not Found
    std::string version = "HTTP/3"; //for images, use HTTP/3
    std::string filepath;
    std::map<std::string, std::string> headers;

    //constructor generates response
    httpResponse(const std::set<std::string>& allowedResources, const httpRequest& req) {
        using namespace std;
        const string& t = req.target;
        if(req.method != GET || allowedResources.find(t) == allowedResources.end()) {
            responseCode = notFound;
            cerr << "client requested illegal resource " << t << '\n';
            return;
        }
        responseCode = "200 OK";
        auto idx = t.find('.');

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

        if(!filesystem::exists(path + t)) {
            responseCode = notFound;
            cerr << "could not find file " << path + t << '\n';
            return;
        }
        filepath = path + t;
        headers["Content-Length:"] = to_string(filesystem::file_size(filepath));


    }
    /**
     * assembles header with blank lines at the end so that the file may
     * be printed immediately after
     * @return  the complete header as a string
     */
    std::string header() {
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

