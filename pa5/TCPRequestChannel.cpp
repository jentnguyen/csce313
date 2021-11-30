#include "TCPRequestChannel.h"

using namespace std;

TCPRequestChannel::TCPRequestChannel (const string host_name, const string port_no) {
    // if channel is server side, host_name is empty string
    if(host_name == "") {
        // server side
        struct addrinfo hints, *serv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        int rv;
        if ((rv = getaddrinfo(NULL, port_no.c_str(), &hints, &serv)) != 0) {
            cerr  << "getaddrinfo: " << gai_strerror(rv) << endl;
            exit(1);
        }
        if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
            perror("server: socket");
            exit(1);
        }
        if (bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            exit(1);
        }
        freeaddrinfo(serv); // all done with this structure

        if (listen(sockfd, 20) == -1) {
            perror("listen");
            exit(1);
        }
    } else {
        // client side
        struct addrinfo hints, *res;

        // first, load up address structs with getaddrinfo():
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int status;
        //getaddrinfo("www.example.com", "3490", &hints, &res);
        if ((status = getaddrinfo (host_name.c_str(), port_no.c_str(), &hints, &res)) != 0) {
            cerr << "getaddrinfo: " << gai_strerror(status) << endl;
            exit(1);
        }

        // make a socket:
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0){
            perror ("Cannot create socket");	
            exit(1);
        }

        // connect!
        if (connect(sockfd, res->ai_addr, res->ai_addrlen)<0){
            perror ("Cannot Connect");
            exit(1);
        }
        //
        cout << "Connected " << endl;
        // now it is time to free the memory dynamically allocated onto the "res" pointer by the getaddrinfo function
        freeaddrinfo (res);
    }
}

TCPRequestChannel::TCPRequestChannel(int _socket) {
    sockfd = _socket;
}

TCPRequestChannel::~TCPRequestChannel() {
    close(sockfd);
}

int TCPRequestChannel::cread(void* msgbuf, int buflen) {
    return recv(sockfd, msgbuf, buflen, 0);
}

int TCPRequestChannel::cwrite(void* msgbuf, int msglen) {
    return send(sockfd, msgbuf, msglen, 0);
}

int TCPRequestChannel::getfd() {
    return sockfd;
}