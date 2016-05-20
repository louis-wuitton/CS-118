#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <vector>
#include <map>
#include "httpmsg.h"
#include <iostream>
#include <sstream>
#include <fstream>


struct Connection
{
    int m_fd;
    std::string m_state;
    std::vector<char> m_recv;
    std::vector<char> m_send;
    std::vector<char> m_pload;
    bool close;
};

std::vector<Connection> m_con;


int findFD(int fd)
{
    for (int i = 0; i < m_con.size(); i++)
    {
        if(m_con[i].m_fd == fd)
            return i;
    }
    return -1;
}

int
main(int argc, char* argv[])
{
    
    fd_set readFds;
    fd_set writeFds;
    fd_set errFds;
    fd_set watchFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errFds);
    FD_ZERO(&watchFds);
    
    char dir[100];
    char def[2]= ".";
    if (argc == 4)
        strcpy(dir, argv[3]);
    else
        strcpy(dir, def);
    
    int listenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    std::string hostname;
    int portno = -1;
    int maxSockfd = listenSockfd;
    Connection listenskt;
    listenskt.close = false;
    listenskt.m_fd = listenSockfd;
    listenskt.m_state = "Listen";
    m_con.push_back (listenskt);
    
    // put the socket in the socket set
    FD_SET(listenSockfd, &watchFds);
    FD_SET(listenSockfd, &readFds);
    
    // allow others to reuse the address
    int yes = 1;
    if (setsockopt(listenSockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        return 1;
    }

    char serverIP[20];
    struct addrinfo hints;
    struct addrinfo* res;
    //prepare hints
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
        //convert address into IPv4 form
        struct sockaddr_in* ipv4 = (struct sockaddr_in*) p->ai_addr;
        
        //convert the ip to a string and print it
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
    
    if (bind(listenSockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 2;
    }
    
    // set the socket in listen status
    if (listen(listenSockfd, 10) == -1) {
        perror("listen");
        return 3;
    }
    
    
    while (true) {
        int nReadyFds = 0;
        errFds = watchFds;
        
        if(!FD_ISSET(listenSockfd, &readFds))
            FD_SET(listenSockfd, &readFds);
        if(!FD_ISSET(listenSockfd, &watchFds))
            FD_SET(listenSockfd, &watchFds);
        
        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, NULL)) == -1) {
            perror("select");
            return 4;
        }
        
        if (nReadyFds == 0) {
            std::cout << "no data is received for 3 seconds!" << std::endl;
            //handle the timeout
            break;
        }
        else {
            for(int fd = 0; fd <= maxSockfd; fd++) {
                // get one socket for reading
                if (FD_ISSET(fd, &readFds)) {
                    if (fd == listenSockfd) { // this is the listen socket
                        struct sockaddr_in clientAddr;
                        socklen_t clientAddrSize = sizeof(clientAddr);
                        int clientSockfd = accept(fd, (struct sockaddr*)&clientAddr, &clientAddrSize);
                        
                        if (clientSockfd == -1) {
                            perror("accept");
                            return 5;
                        }
                        
                        char ipstr[INET_ADDRSTRLEN] = {'\0'};
                        inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
                        std::cout << "Accept a connection from: " << ipstr << ":" <<
                        ntohs(clientAddr.sin_port) << " with Socket ID: " << clientSockfd << std::endl;
                        
                        //push back this socket
                        Connection m_connected;
                        m_connected.close = false;
                        m_connected.m_fd = clientSockfd;
                        m_connected.m_state = "Connected";
                        m_con.push_back(m_connected);
                        
                        
                        // update maxSockfd
                        if (maxSockfd < clientSockfd)
                            maxSockfd = clientSockfd;
                        
                        // add the socket into the socket set
                        FD_SET(clientSockfd, &readFds);
                        FD_SET(clientSockfd, &watchFds);
                    }
                    else {
                        char buf[300];
                        int recvLen = 0;
                        int index = findFD(fd);
                        char filepath[100] = {'\0'};
                        strcpy(filepath, dir);
                        if(index == -1)
                        {
                            std::cerr << "can't find the file descriptor" << std::endl;
                            return 1;
                        }
                        memset(buf, '\0', sizeof(buf));
                        if(m_con[index].m_state == "Connected")
                        {
                            m_con[index].m_state = "Reading";
                            FD_SET(fd, &readFds);
                        }
                        struct timeval tv;
                        tv.tv_sec = 30;
                        tv.tv_usec = 0;
                        setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
                        if ((recvLen = recv(fd, buf, 500, 0)) == -1) {
                            perror("recv");
                            close(fd);
                            std::cout << "Timeout expires"<<std::endl;
                            return 6;
                        }

                        for (int i =0; i < recvLen; i++)
                        {
                            m_con[index].m_recv.push_back(buf[i]);
                            if(buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
                            {
                                m_con[index].m_recv.push_back(buf[i+1]);
                                m_con[index].m_recv.push_back(buf[i+2]);
                                m_con[index].m_recv.push_back(buf[i+3]);
                                m_con[index].m_state = "Writing";
                                break;
                            }
                            
                        }

                        if (m_con[index].m_state == "Writing"){
                            std::cout << "Received the message "<<std::endl;
                            for (int i =0; i<m_con[index].m_recv.size(); i++) {
                                std::cout << m_con[index].m_recv[i];
                            }
                            std::cout << std::endl;
                            FD_CLR(fd, &readFds);
                            std::vector<char> pload;
                            HttpRequest request;
                            request.decodeFirstLine(m_con[index].m_recv);
                            request.decodeHeaderline(m_con[index].m_recv);
                            HttpResponse response;
                            response.setVersion(request.getVersion());
                            HttpStatus status;
                            bool set_status = false;
                            
                            if(request.getMap("Connection") == "keep-alive")
                                response.setHeader("Connection", "keep-alive");
                            else if(request.getMap("Connection") == "close")
                                response.setHeader("Connection", "close");
                            else
                            {
                                if(set_status == false){
                                    status.m_statuscode = "400";
                                    response.setStatus(status);
                                    response.setDescription ("Bad Request");
                                    response.setHeader("Content-Length", "0");
                                    set_status = true;
                                    FD_SET(fd, &writeFds);
                                    m_con[index].m_state = "Writing";
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
                                    while (1){
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
                                        FD_SET(fd, &writeFds);
                                        m_con[index].m_state = "Writing";
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
                                        FD_SET(fd, &writeFds);
                                        m_con[index].m_state = "Writing";
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
                                            FD_SET(fd, &writeFds);
                                            m_con[index].m_state = "Writing";
                                        }
                                    }
                                    else
                                    {
                                        char host_check[200] = {'\0'};
                                        int pt_check = -1;
                                        int j = 0;
                                        i++;
                                        while (i < len)
                                        {
                                            if(hosturl[i]!= '/'&&hosturl[i]!= ':')
                                            {
                                                host_check[j] = hosturl[i];
                                                j++;
                                                i++;
                                            }
                                            else
                                                break;
                                        }
                                        std::string mycheck = host_check;
                                        if(hosturl[i] == ':')
                                        {
                                            //if we hit a colon, check for port number
                                            j = 0;
                                            char po_no[100] = {'\0'};
                                            while (hosturl[i]!='/'){
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
                                                FD_SET(fd, &writeFds);
                                                m_con[index].m_state = "Writing";
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
                                    FD_SET(fd, &writeFds);
                                    m_con[index].m_state = "Writing";
                                }
                                
                            }
                            else if (request.getMethod().m_methodstr != "GET") {
                                if(set_status == false){
                                    status.m_statuscode = "405";
                                    response.setStatus(status);
                                    response.setDescription ("Method not allowed");
                                    response.setHeader("Content-Length", "0");
                                    set_status = true;
                                    FD_SET(fd, &writeFds);
                                    m_con[index].m_state = "Writing";
                                }
                            }
                            else if (set_status == false)
                            {
                                std::ifstream file;
                                file.exceptions( std::ifstream::badbit | std::ifstream::eofbit);
                            
                                file.open(filepath, std::ifstream::in | std::ifstream::binary | std::ifstream::out);
                                if(!file.is_open())
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
                                    out << pload.size();
                                    response.setHeader("Content-Length", out.str());
                                    
                                }
                                m_con[index].m_pload = pload;
                                
                                memset(filepath, '\0', 100);
                                FD_SET(fd, &writeFds);
                                m_con[index].m_state = "Writing";
                            }
                            if (response.getMap("Connection") == "close") {
                                m_con[index].close = true;
                            }
                            m_con[index].m_recv.clear();
                            m_con[index].m_send = response.encode();
                        }
                    }
                    }
                else if (FD_ISSET(fd, &writeFds)){
                    int index = findFD(fd);
                    if(m_con[index].m_state == "Writing"){
                        char buf[500];
                        int sendLen = 0;
                        memset(buf, '\0', sizeof(buf));
                        if(!m_con[index].m_send.empty())
                        {
                            int sendlen = m_con[index].m_send.size();
                            for (int i = 0; i < sendlen; i++)
                            {
                                buf[i] = m_con[index].m_send[0];
                                m_con[index].m_send.erase(m_con[index].m_send.begin());
                            }
                            if ((sendLen = send(fd, buf, sendlen, 0)) == -1) {
                                perror("send");
                                return 4;
                            }
                        }
                        else
                        {
                            int size;
                            for (size = 0; size < 500; size++)
                            {
                                if(m_con[index].m_pload.empty())
                                    break;
                                buf[size] = m_con[index].m_pload[0];
                                m_con[index].m_pload.erase(m_con[index].m_pload.begin());
                            }
                            
                            if ((sendLen = send(fd, buf, size, 0)) == -1) {
                                perror("send");
                                return 4;
                            }

                        }
                        
                        if(m_con[index].m_send.empty() && m_con[index].m_pload.empty())
                        {
                            m_con[index].m_state = "Done";
                            FD_CLR(fd, &writeFds);
                            if (m_con[index].close == true)
                            {
                                std::cout <<"Close the socket " << fd <<std::endl;
                                FD_CLR(fd, &watchFds);
                                m_con[index].m_fd = -1;
                                close(fd);
                            }
                            else
                            {
                                FD_SET(fd, &readFds);
                                m_con[index].m_state = "Reading";
                            }
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}