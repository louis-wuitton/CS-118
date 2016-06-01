#include <string>
#include <string.h>
#include <thread>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <sstream> 
#include <map>
#include <vector>
#include <stdio.h>
#include "httpmsg.h"
#include <chrono>


std::string hostname;
int portno = 0;

void do_child(int fd, char* dir){
    int recvLen = 0;
    int p_size = 0;
    std::vector<char> pload;
    char buf[500] = {0};
    memset(buf, '\0', sizeof(buf));
    
    char filepath[100];
    
    struct timeval tv;
    tv.tv_sec = 40;
    tv.tv_usec = 0;
    setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
    std::vector <char> wire;
    std::vector <char> res_wire;
    
    while (true){
    strcpy(filepath, dir);
    bool isdone = false;
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
    while(1){
        memset(buf, '\0', sizeof(buf));
        if ((recvLen = recv(fd, buf, 500, 0)) == -1) {
            std::cout << "Timeout expires" << std::endl;
            perror("recv");
            close(fd);
            return;
        }
        for (int i =0; i < recvLen; i++)
        {
            wire.push_back(buf[i]);
            if(buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            {
                wire.push_back(buf[i+1]);
                wire.push_back(buf[i+2]);
                wire.push_back(buf[i+3]);
                isdone = true;
                break;
            }
        }
        if(isdone == true)
            break;
    }
        
        std::cout << "Received the request message: "<<std::endl;
        for (int i = 0; i<wire.size(); i++) {
            std::cout << wire[i];
        }
        std::cout << std::endl;
        
        HttpRequest request;
        request.decodeFirstLine(wire);
        request.decodeHeaderline(wire);
        wire.clear();
        HttpResponse response;
        response.setVersion(request.getVersion());
        HttpStatus status;
        bool set_status = false;
    
        if(request.getMap("Connection") == "Keep-Alive")
            response.setHeader("Connection", "Keep-Alive");
        else if(request.getMap("Connection") == "Close")
            response.setHeader("Connection", "Close");
        else
        {
            if(set_status == false){
            status.m_statuscode = "400";
            response.setStatus(status);
            response.setDescription ("Bad Request");
            response.setHeader("Content-Length", "0");
            set_status = true;
            }
        }
    
        std::string recurl = request.getUrl();

        int len = recurl.length();
        int i = 0;
    
        if(recurl[i] == 'h'&& recurl[i+1] == 't' && recurl[i+2] == 't' && recurl[i+3] == 'p' &&recurl[i+4] ==':' && recurl[i+5]=='/' &&recurl[i+6] == '/')
        {
            i+=7;
            char host_check[200] = {'\0'};
            int pt_check = -1;
            int j = 0;
            while (i < len)
            {
                if(recurl[i]!= '/' && recurl[i]!= ':')
                {
                    host_check[j] = recurl[i];
                    j++;
                    i++;
                }
                else
                    break;
            }
            std::string mycheck = host_check;
            if(recurl[i] == ':')
            {
                j = 0;
                i++;
                char po_no[100];
                while (i < len){
                    if(recurl[i] == '/')
                        break;
                    po_no[j] = recurl[i];
                    i++;
                    j++;
                }
                pt_check = atoi(po_no);
            }
            if(mycheck != hostname || (portno != pt_check))
            {
                if(set_status == false){
                status.m_statuscode = "400";
                response.setStatus(status);
                response.setDescription ("Bad Request");
                response.setHeader("Content-Length", "0");
                set_status = true;
                }
            }
            else
                strcat(filepath, recurl.substr(i).c_str());
        }
        else
        {
            if(recurl[i] != '/')
            {
                if(set_status == false){
                status.m_statuscode = "400";
                response.setStatus(status);
                response.setDescription ("Bad Request");
                response.setHeader("Content-Length", "0");
                set_status = true;
                }
            }
            else
            {
                std::string hosturl = request.getMap("Host");
                if (hosturl == "Cannot find this key") {
                    if(set_status == false){
                    status.m_statuscode = "400";
                    response.setStatus(status);
                    response.setDescription ("Bad Request");
                    response.setHeader("Content-Length", "0");
                    set_status = true;
                    }
                }
                else
                {
                    char host_check[200] = {'\0'};
                    int pt_check = -1;
                    i = 0;
                    while (i < hosturl.length())
                    {
                        if(hosturl[i]!= '/' && hosturl[i]!= ':')
                        {
                            host_check[i] = hosturl[i];
                            i++;
                        }
                        else
                            break;
                    }
                    std::string mycheck = host_check;
                    if(hosturl[i] == ':')
                    {
                        //if we hit a colon, check for port number
                        int j = 0;
                        i++;
                        char po_no[100] = {'\0'};
                        while (i < hosturl.length()){
                            po_no[j] = hosturl[i];
                            i++;
                            j++;
                        }
                        pt_check = atoi(po_no);
                    }
                    if(mycheck != hostname || (portno != pt_check))
                    {
                        if(set_status == false){
                        status.m_statuscode = "400";
                        response.setStatus(status);
                        response.setDescription ("Bad Request");
                        response.setHeader("Content-Length", "0");
                        set_status = true;
                        }
                    }
                    else
                        strcat(filepath, recurl.c_str());

                }
            }
        }
    
        
    if(strcmp(request.getVersion().m_versionstr.c_str(), "HTTP/1.0" ) != 0 && strcmp(request.getVersion().m_versionstr.c_str(), "HTTP/1.1" ) != 0)
    {
        if(set_status == false){
        status.m_statuscode = "505";
        response.setStatus(status);
        response.setDescription ("HTTP version not supported");
        response.setHeader("Content-Length", "0");
        set_status = true;
        }
    }

    else if (request.getMethod().m_methodstr != "GET") {
        if(set_status == false){
        status.m_statuscode = "405";
        response.setStatus(status);
        response.setDescription ("Method not allowed");
        response.setHeader("Content-Length", "0");
        set_status = true;
        }
    }
    else if(set_status == false)
    {
        std::ifstream file;
        file.exceptions(
                        std::ifstream::badbit
                        | std::ifstream::eofbit);
        
        file.open(filepath, std::ifstream::in | std::ifstream::binary | std::ifstream::out);

        if (!file.is_open())
        {
            status.m_statuscode = "404";
            response.setStatus(status);
            response.setDescription ("Not Found");
            response.setHeader("Content-Length", "0");
            set_status = true;
        }
        else
        {
            status.m_statuscode = "200";
            response.setStatus(status);
            response.setDescription ("OK");
            set_status = true;
            file.seekg(0, std::ios::end);
            std::streampos length(file.tellg());
            if (length) {
                file.seekg(0, std::ios::beg);
                pload.resize(static_cast<std::size_t>(length));
                file.read(&pload.front(), static_cast<std::size_t>(length));
            }
            file.close();
            response.setPayLoad(pload);
            std::stringstream out;
            p_size = pload.size();
            out << p_size;
            response.setHeader("Content-Length", out.str());
        }
        memset(filepath, '\0', 100);
    }
    res_wire = response.encode();
    int wsize = res_wire.size();
    
    
    memset(buf, '\0', sizeof(buf));
    for (int k1 = 0; k1 < wsize; k1++) {
        buf[k1] = res_wire[k1];
    }
    
    res_wire.clear();
        
    if (send(fd, buf, wsize, 0) == -1) {
        perror("send");
        exit(1);
    }
    
    if(response.getStatus().m_statuscode == "200")
    {
        int divide = p_size/500;
        int remainder = p_size % 500;
        int count = 0;
        for (int k = 0; k < divide; k++) {
            memset(buf, '\0', sizeof(buf));
            for (int k1 = 0; k1 < 500; k1++) {
                buf[k1] = pload[count];
                count ++;
            }

            if (send(fd, buf, 500, 0) == -1) {
                perror("send");
                exit(1);
            }
        }
        if(remainder!=0)
        {
            memset(buf, '\0', sizeof(buf));
            for(int q = 0; q < remainder; q++)
            {
                buf[q] = pload[count];
                count++;
            }
           
            if (send(fd, buf, remainder, 0) == -1) {
                perror("send");
                exit(1);
            }
        }
    }
    pload.clear();
    if(response.getMap("Connection") == "close")
    {
        std::cout << "successfully close the connection" <<std::endl;
        close(fd);
        break;
    }
}

}

int main(int argc, char* argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        return 1;
    }
    
    char serverIP[20];
    struct addrinfo hints;
    struct addrinfo* res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    //get address
    int status = 0;
    
    if(argc == 1)
    {
        hostname = "localhost";
        portno = 4000;
    }
    else
    {
        hostname = argv[1];
        portno = atoi(argv[2]);
    }
    
    if((status=getaddrinfo(hostname.c_str(), "80", &hints, &res))!=0)
    {
        std::cerr<<"getaddrinfo: " << gai_strerror(status) << std::endl;
        exit(1);
    }
    
    for(struct addrinfo* p = res; p != 0; p = p->ai_next)
    {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) p->ai_addr;
        char ipstr[INET_ADDRSTRLEN] = {'\0'};
        inet_ntop (p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
        std::cout << "  " << ipstr << std::endl;
        strcpy(serverIP, ipstr);
        ntohs(ipv4->sin_port);
    }
    
    // bind address to socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portno);     // short, network byte order
    addr.sin_addr.s_addr = inet_addr(serverIP);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
    
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 2;
    }
    
    //set the socket into listening state
    if (listen(sockfd, 10) == -1) {
        perror("listen");
    }

    
    while(1){
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int clientSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);
    
        if (clientSockfd == -1) {
            perror("accept");
            return 4;
        }
        char ipstr[INET_ADDRSTRLEN] = {'\0'};
        inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
        std::cout << "Accept a connection from: " << ipstr << ":" <<
        ntohs(clientAddr.sin_port) << std::endl;
       
        char def[2]= ".";
        
        if (argc == 4)
            std::thread(do_child, clientSockfd, argv[3]).detach();
        else
            std::thread(do_child, clientSockfd, def).detach();
    }
    return 0;
}
