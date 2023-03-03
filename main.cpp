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

// Joan Omley
// CSPC 5042 week7 - MVP HTTP responses
// Referenced from https://www.codingninjas.com/codestudio/library/learning-socket-programming-in-c
// and https://www.youtube.com/watch?v=Pg_4Jz8ZIH4 with professor Riley's split() function.

using namespace std;

#define TRUE   1
#define PORT 8080
#define SERVER_BACKLOG 1
#define BUFF_SIZE 1025
#define MAX_PATH_LENGTH 500;

int pipe_fds[2];

vector<string> split(string& src, char delim);
void *processSocket(void *params);
void *runLogger(void *params);
string convertToString(char* a, int size);
void send404(string,string,int);
bool isPNG(string);

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

        if ((client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size))<0)
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

    // if the uri is /quit will shut down the server
    if (req.uri == "quit") {
        send404(req.uri,req.verb,client_socket);
        sleep(1);
        exit(0);
    }

    // gets content type from the struct
    vector<string> content_type = split(req.uri, '.');
    if (content_type[1] == "html") {content_type[1] = "text/html";}
    else if (content_type[1] == "css") {content_type[1] = "text/css";}
    else if (content_type[1] == "js") {content_type[1] = "text/js";}
    else if (content_type[1] == "png") {content_type[1] = "image/png";}
    else if (content_type[1] == "ico") {content_type[1] = "image/ico";}
    bool PNG = isPNG(content_type[1]);

    // gets the file and creates a buffer to read from the file
    ifstream theFile(req.uri);

    // processes data if not given /quit instruction and if the file exists
    if (theFile) {
        // creates a stream to add contents of file to
        stringstream body;
        body << theFile.rdbuf();

        // sends what the response will be to the logger
        int fileSize = body.str().size();
        string content = req.verb + " /" + req.uri + " 200 OK content-type: " + content_type[1] + " content-size: " +
            to_string(fileSize) + "\n";
        int content_size = content.length();
        write(pipe_fds[1],content.c_str(), content_size + 1);

        // creates a http response with the file requested file contents
        string response = "HTTP/1.0 200 OK\r\n"
                          "Server: Joan's Server/1.0\n"
                          "Content-Length: " + to_string(fileSize) + "\r\n" +
                          "Content-Type: " + content_type[1] + "\r\n" +
                          "\r\n" + body.str() ;

        // sends the request back to the client
        send(client_socket, response.c_str(), response.length(),0);
    }

    // if not /quit or if there is no file to be found (404) sends 404
    else {
        send404(req.uri,req.verb,client_socket);
    }

    return NULL;
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