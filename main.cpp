#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pthread.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include "string"
#include "sstream"
#include "iostream"
#include "fstream"
#include "algorithm"
#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>

// Joan Omley
// CSPC 5042 final 
// Referenced from https://www.codingninjas.com/codestudio/library/learning-socket-programming-in-c
// and https://www.youtube.com/watch?v=Pg_4Jz8ZIH4 with professor Riley's split() function.

using namespace std;

#define TRUE   1
#define PORT 8080
#define SERVER_BACKLOG 1
#define BUFF_SIZE 1025
#define MAX_PATH_LENGTH 500;

extern char** environ;

int pipe_fds[2];
const char * mapped_shakespeare;

vector<string> split(string& src, char delim);
void *processSocket(void *params);
void *runLogger(void *params);
string convertToString(char* a, int size);
void send404(string,string,int);
void quit(string,string,int);
bool isPNG(string);
string getEnvResponse();
void mapShakespeare();
void writeToLogger(int fileSize, string verb, string uri, string request_type);
void sendResponse(int client_socket, int fileSize, string request_type, string body);
string formatRequestType(vector<string> content_type);
void send400 (string uri, string verb, int socket);

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

// header struct
typedef struct {
    string key;
    string value;
} http_header_t;

// response struct
typedef struct {
    string verb;
    string version;
    string uri;
    vector<http_header_t> headers;
} http_request_t;

int main(int argc , char *argv[])
{
    int server_socket, client_socket, addr_size;
    SA_IN server_addr, client_addr;

    // creates the server
    if( (server_socket = socket(AF_INET, SOCK_STREAM,0)) == 0)
    {
        perror("Failed_Socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // creates the sockets
    if (bind(server_socket, (SA*)&server_addr, sizeof(server_addr))<0)
    {
        perror("Failed_Bind");
        exit(EXIT_FAILURE);
    }

    // listens on sockets
    if (listen(server_socket, SERVER_BACKLOG) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    addr_size = sizeof(server_addr);

    // creates the logger
    pthread_t logger;
    printf("Logger pid: %li Logger tid: %i\n",pthread_self(),gettid());
    pthread_create(&logger, 0, runLogger, NULL);


    // loop accepts connections to sockets and runs processSocket() on them
    while (TRUE) {

        if ((client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size)) < 0)
        {
            perror("Accept!");
            exit(EXIT_FAILURE);
        }

        pthread_t t;
        int *pclient = &client_socket;
        pthread_create(&t,NULL, processSocket, pclient);
    }
}

// handles incoming http requests from sockets
void * processSocket(void* socket) {
    // get socket number
    int client_socket = *((int*)socket);
    char buffer[BUFF_SIZE];

    // read data from socket
    int value_read = read( client_socket, buffer, 1024);

    // creates the request struct
    http_request_t req;

    // converts buffer to string
    string s_req = "";
    for (int i = 0; i < sizeof(buffer); i++) {
        s_req += buffer[i];
    }

    // parses the request
    vector<string> verb = split(s_req, ' ');
    vector<string> vers = split(verb[2], '/');
    vector<string> text = split(s_req, '\n');

    // assigns parsed values to request struct
    req.verb = verb[0];
    req.uri = verb[1];
    req.uri.erase(remove(req.uri.begin(), req.uri.end(), '/'), req.uri.end());
    req.version = split(vers[1],'\n')[0];

    // checks for parameters
    vector<string> parameters = split(req.uri, '?');
    req.uri = parameters[0];

    // gets content type from the struct and formats it
    vector<string> content_type = split(req.uri, '.');


    // if the uri is /quit will shut down the server
     if (req.uri == "quit") {
        quit(req.uri,req.verb,client_socket);
        sleep(1);
        exit(0);
    }

    // if uri is /env gets environment variables
    else if (req.uri == "env") {

        string response = getEnvResponse();
        int fileSize = response.length();
        string content = req.verb + " /" + req.uri + " 200 OK content-type: text/html content-size: " +
                         to_string(fileSize) + "\n";
        int content_size = content.length();
        write(pipe_fds[1], content.c_str(), content_size + 1);
        send(client_socket, response.c_str(), response.length(), 0);

    }

    // if the request is for shakespeare
    else if (req.uri == "shakespeare.txt" || req.uri == "shakespeare") {
        // response type will always be text
        string request_type = "text";

        // if shakespeare.txt hasn't been mapped yet, it will map it
        if (mapped_shakespeare == NULL) mapShakespeare();

        // if parameters is smaller than 2, that means there are no parameters and the whole file should be read
        // sends data to logger and response client
        if (parameters.size() < 2) {
             int fileSize = strlen(mapped_shakespeare);
             writeToLogger(fileSize, req.verb, req.uri, request_type);
             sendResponse(client_socket, fileSize, request_type, mapped_shakespeare);
        }
        // else there are parameters and the parameters are split
        else {
             vector<string> separate = (split(parameters[1], '&'));
             // there should only be 2 parameters, and if there aren't, sends bad request
             if (separate.size() != 2) {
                 send400(req.uri,req.verb,client_socket);
             }
             // if there's 2 parameters, starts parsing them out
             else {
                 vector<string> start_param = (split(separate[0], '='));
                 vector<string> length_param = (split(separate[1], '='));
                 int start_pos = stoi(start_param[1]);
                 int length = stoi(length_param[1]);
                 // if params don't match properly sends error 400
                 if (start_param[0] != "start" || length_param[0] != "length") {
                     send400(req.uri,req.verb,client_socket);
                 }
                 // else copies the desired string from the map and sends responses to logger and client
                 else {
                     string response = "";
                     for (int i = start_pos; i < start_pos + length; i++) {
                         response += mapped_shakespeare[i];
                     }
                     int fileSize = response.length();
                     req.uri = parameters[0] + "?" + parameters[1];
                     writeToLogger(fileSize, req.verb, req.uri, request_type);
                     sendResponse(client_socket, fileSize, request_type, response);
                 }
             }
        }
     }
    // if not quit, env, or shakespeare
    else {
        // reformatts the request type for the response
         string request_type = formatRequestType(content_type);

        // gets the file and creates a buffer to read from the file
        ifstream theFile(req.uri);

        // processes data if the file exists
        if (theFile) {
            // creates a stream to add contents of file to
            stringstream body;
            body << theFile.rdbuf();

            // sends what the response will be to the logger
            int fileSize = body.str().size();
            writeToLogger(fileSize,req.verb,req.uri,request_type);

            // creates a http response with the file requested file contents
            sendResponse(client_socket,fileSize,request_type,body.str());

        }
       // if there is no file to be found (404) sends 404
        else {
            send404(req.uri,req.verb,client_socket);
        }
    }

    return NULL;
}

// this takes vector of strings that has been created with split() and converts it to a more apporpriate format
// for the http response
string formatRequestType(vector<string> content_type) {
    if (content_type.size() > 1) {
        if (content_type[1] == "html") return "text/html";
        else if (content_type[1] == "css") return "text/css";
        else if (content_type[1] == "js") return "text/js";
        else if (content_type[1] == "png") return "image/png";
        else if (content_type[1] == "ico") return "image/ico";
        else if (content_type[1] == "txt") return "text";
        else return "";
    }
    else return content_type[0];
}

// creates an http response and sends it to the client
void sendResponse(int client_socket, int fileSize, string request_type, string body) {
    string response = "HTTP/1.0 200 OK\r\n"
                      "Server: Joan's Server/1.0\n"
                      "Content-Length: " + to_string(fileSize) + "\r\n" +
                      "Content-Type: " + request_type + "\r\n" +
                      "\r\n" + body;

    send(client_socket, response.c_str(), response.length(), 0);
}

// creates the data for the logger and writes it to the pipe the logger reads from
void writeToLogger(int fileSize, string verb, string uri, string request_type) {

    string content = verb + " /" + uri + " 200 OK content-type: " + request_type + " content-size: " +
                     to_string(fileSize) + "\n";

    int content_size = content.length();
    write(pipe_fds[1], content.c_str(), content_size + 1);
}

// maps shakespeare.txt to memory which can then be sent to the client, or a chunk can be pulled from it and sent
// mostly taken from Professor Riley's example in the assignment
void mapShakespeare() {
    int fd;
    struct stat s;
    int status;
    size_t size;
    const char * file_name = "shakespeare.txt";
    int i;

    fd = open(file_name, O_RDONLY);
    status = fstat(fd, &s);
    size = s.st_size;
    mapped_shakespeare = static_cast<const char*>(mmap(0,size,PROT_READ, MAP_PRIVATE, fd, 0));
}

// creates environment variable response
// memory usage code and the basic env code copied from Prof Riley's example in the assignment
string getEnvResponse() {

    // get memory usage
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    int max_rss_mb = usage.ru_maxrss / 1024;    // Max RSS in megabytes

    // create stream and initial html stuff
    stringstream html_output;
    html_output << "<html><body><h1>Environment variables</h1>\n";
    html_output << "<h2>Memory</h2><p>Max size so far: " + to_string(max_rss_mb) + "<h2>Variables</h2>"
                   "<table border='1' style='table-layout:fixed;width:100%;"
                   "font-family:monospace;line-break:anywhere'><colgroup><col style='width:200px'>"
                   "<col style='width:auto'></colgroup><tr><th>Var</th><th>Value</th></tr>";

    // gets environment variables
    char **var = environ;

    // loops throught arrays of environ array
    for (; *var; var++) {
        // converts array to string and splits values on = to get variable and value
        string env_var = string(*var);
        vector<string> separate = split(env_var,'=');

        string variable = separate[0];
        string value = "";
        // checks size of the split vector in case variable has no paired value
        if (separate.size() > 1) {
            value = separate[1];
        }
        // adds to the stream with html markups
        html_output << "<tr><td>" << variable << "</td><td>" << value << "</td></tr>";

//        html_output << env_var <<  std::endl;
    }
    html_output << "</table></body></html>" << endl;

    // gets content length of env variables
    html_output.seekg(0, ios::end);
    int fileSize = html_output.tellg();
    html_output.seekg(0,ios::beg);

    // http response
    string response = "HTTP/1.0 200 OK\r\n"
                      "Server: Joan's Server/1.0\n"
                      "Content-Length: " + to_string(fileSize) + "\r\n" +
                      "Content-Type: text/html" + "\r\n" +
                      "\r\n" + html_output.str();

    return response;
}

// checks if it is a PNG. used in older version
bool isPNG(string contenttype) {
    return contenttype == "image/png";
}

// sends error 404 to the logger and the socket
void send404 (string uri, string verb, int socket) {
    string shutdown = verb +" /" + uri + " 404 File not found\r\n";
    string error404 = "HTTP/1.0 404 File not found\r\n\r\n";
    write(pipe_fds[1],shutdown.c_str(), shutdown.length());
    write(socket, error404.c_str(), error404.length());
}

// sends error 400 to the logger and the socket
void send400 (string uri, string verb, int socket) {
    string shutdown = verb +" /" + uri + " 400 Bad request\r\n";
    string error404 = "HTTP/1.0 400 Bad request\r\n\r\nBad Request\r\n";
    write(pipe_fds[1],shutdown.c_str(), shutdown.length());
    write(socket, error404.c_str(), error404.length());
}

void quit (string uri, string verb, int socket) {
    string shutdown = verb +" /" + uri + " 200 OK Shutting down\r\n";
    string response = "HTTP/1.0 200 OK\r\n\r\nShutting Down\r\n";
    write(pipe_fds[1],shutdown.c_str(), shutdown.length());
    write(socket, response.c_str(), response.length());
}

// logger that all threads send text to via a pipe and prints them
void *runLogger(void *params) {
    ::printf("Listening on port %i\n",PORT);
    char buffer[BUFF_SIZE];
    ssize_t bytes = read(pipe_fds[0],buffer, sizeof(buffer));
    while (TRUE) {
        int i = 0;
        while (i < bytes) {
            ::printf("%c",buffer[i]);
            i++;
        }
    }
}

// converts a char array to a string. not used in this version
string convertToString(char* a, int size)
{
    int i;
    string s = "";
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}

// Prof Riley's split function that splits a string
vector<string> split(string& src, char delim) {
    istringstream ss(src);
    vector<string> res;

    string piece;
    while(getline(ss, piece, delim)) {
        res.push_back(piece);
    }

    return res;
}