#include <string>
#include <thread>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "packet.h"


using namespace std;

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        cerr << "Invalid number of arguments"<<std::endl;
        return 1;
    }
    //creating an UDP socket
    
    fd_set readFds;
    fd_set writeFds;
    fd_set errFds;
    fd_set watchFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errFds);
    FD_ZERO(&watchFds);
    
    
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Can't create socket");
        return 1;
    }
    
    int maxSockfd = sockfd;
    char serverIP[20];
    struct addrinfo hints;
    struct addrinfo* res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    
    int portno = atoi(argv[1]);
    cout << "port number is " << portno<<endl;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portno);     // short, network byte order
    addr.sin_addr.s_addr = 0; //inet_addr("10.0.0.1");
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
    
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 2;
    }
    
    HeaderPacket req_pkt;
    
    struct sockaddr_in remote_addr;
    socklen_t addrlen = sizeof(remote_addr);
    
    struct sockaddr_in clientAddr;
    int clilen = sizeof(clientAddr);
    char ipstr[INET_ADDRSTRLEN] = {'\0'};
    inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
    std::cout << "Accept a connection from: " << ipstr << ":" <<
    ntohs(clientAddr.sin_port) << std::endl;
    
    
    while (true) {
        int nReadyFds = 0;
        if(!FD_ISSET(sockfd, &watchFds))
            FD_SET(sockfd, &watchFds);
        if(!FD_ISSET(sockfd, &readFds))
            FD_SET(sockfd, &readFds);
        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, NULL)) == -1) {
            perror("select");
            return 4;
        }
        errFds = watchFds;
        if (nReadyFds == 0) {
            std::cout << "no data is received" << std::endl;
            //handle the timeout
            break;
        }
        else
        {
            cout <<"Select received something" <<endl;
            for(int fd = 0; fd <= maxSockfd; fd++){
                if (FD_ISSET(fd, &readFds)) {
                    cout << "Read something "<< endl;
                    HeaderPacket req_pkt;
                    int clilen;
                    struct sockaddr_in clientAddr;
                    if(recvfrom(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &clientAddr, (socklen_t*) &clilen) < 0)
                    {
                        cerr<<"recvfrom failed" <<endl;
                        return 1;
                    }
                    char ipstr[INET_ADDRSTRLEN] = {'\0'};
                    inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
                    std::cout << "Accept a connection from: " << ipstr << ":" <<
                    ntohs(clientAddr.sin_port) << std::endl;
                    
                }
            }
        }
    }
    
}
