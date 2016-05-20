#include <string.h>
#include <thread>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <map>
#include <vector>
#include "httpmsg.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <getopt.h>



int persistent_flag;
int pipelining_flag;
int sockfd;

static struct option long_options[]=
{
    {"persistent", no_argument, &persistent_flag, 1},
    {"pipelining", no_argument, &pipelining_flag, 1},
    {0, 0, 0, 0}
};

void do_connect(char* hostname, int portno)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    char serverIP[20];
    struct addrinfo hints;
    struct addrinfo* res;
    
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    int status = 0;
    if((status=getaddrinfo(hostname, "80", &hints, &res))!=0)
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
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(portno);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
    
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("connect");
        exit(1);
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) {
        perror("getsockname");
        exit(3);
    }
    
    char ipstr[INET_ADDRSTRLEN] = {'\0'};
    //getting addresses of client
    inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
    std::cout << "Set up a connection from: " << ipstr << ":" <<
    ntohs(clientAddr.sin_port) << std::endl;
}



void doHttp(std::string url, int current, int total)
{
    char hostname[500];
    int i = 0;
    while(url[i]!= '/')
        i++;
    if (url[i+1] == '/')
        i+=2;
    else
    {
        std::cerr<<"The first '/' in url is not followed by another '/', thus the url is invalid"<<std::endl;
        exit(1);
    }
    int j = 0;
    while(i < url.length())
    {
        if(url[i] =='/' || url[i]==':')
            break;
        hostname[j] = url[i];
        i++;
        j++;
    }
    hostname[j] = '\0';
    j = 0;
    int portno = -1;
    if(url[i] == ':')
    {
        char ptno[10];
        i++;
        while(i < url.length())
        {
            if(url[i]=='/')
                break;
            ptno[j] = url[i];
            i++;
            j++;
        }
        ptno[j] = '\0';
        portno = atoi(ptno);
    }
    else
    {
        std::string hstname = hostname;
        if (hstname == "localhost") {
            portno = 4000;
        }
        portno = 80;
    }

    i++;
    j=0;
    
    char fname[100] = {'\0'};
    while(url[i] != '\0')
    {
        fname[j] = url[i];
        j++;
        i++;
    }
    
    if((persistent_flag==0) || (persistent_flag==1 && current == 1))
        do_connect(hostname, portno);
        
    HttpRequest request;
    request.setUrl(url);
    HttpMethod method;
    method.m_methodstr = "GET";
    HttpVersion version;
    if(persistent_flag)
    {
        if(current == total)
            request.setHeader("Connection", "close");
        else
            request.setHeader("Connection", "keep-alive");
        version.m_versionstr = "HTTP/1.1";
    }
    else
    {
        request.setHeader("Connection", "close");
        version.m_versionstr = "HTTP/1.0";
    }
    request.setVersion(version);
    request.setMethod(method);
    std::vector<char> wire = request.encode();
    
    std::cout << "Send out request: "<<std::endl;
    for (int i =0; i<wire.size(); i++) {
        std::cout << wire[i];
    }
    std::cout<< std::endl;
    
    char buf[500] = {0};
    memset(buf, '\0', sizeof(buf));
    
    for (int k =0; k < wire.size(); k+=500){
        memset(buf, '\0', sizeof(buf));
        //fill the buf with things in wire
        for (int k1 = 0; k1 < 500; k1++)
        {
            buf[k1] = wire[k1+k*500];
        }
        if (send(sockfd, buf, 500, 0) == -1) {
            perror("send");
            exit(4);
        }
    }
    
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
    
    std::vector <char> rec_wire;
    std::vector <char> payload;
    int recvLen;
    int rest=0;
    bool isdone = false;
    int pld_start;
    memset(buf, '\0', sizeof(buf));
    //receiving the message
    
    while(1){
        memset(buf, '\0', sizeof(buf));
        if ((recvLen = recv(sockfd, buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            std::cout << "Timeout expires"<<std::endl;
            close(sockfd);
            exit(1);
        }

        for (int i =0; i < recvLen; i++)
        {
            rec_wire.push_back(buf[i]);
            if(buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            {
                rec_wire.push_back(buf[i+1]);
                rec_wire.push_back(buf[i+2]);
                rec_wire.push_back(buf[i+3]);
                for (int j = i+4; j < recvLen; j++)
                {
                    payload.push_back(buf[j]);
                    rest++;
                }
                isdone = true;
                break;
            }
        }
        if(isdone == true)
            break;
    }
    HttpResponse response;
    response.decodeFirstLine(rec_wire);
    response.decodeHeaderline(rec_wire);
    
    std::cout << "Receive the response message: "<<std::endl;
    for (int q = 0; q < rec_wire.size(); q++)
        std::cout << rec_wire[q];
    int p_size = -1;
    if(response.getMap("Content-Length") != "Cannot find this key")
    {
        p_size = stoi(response.getMap("Content-Length"));
    }
    if (p_size == -1)
    {
        while (true) {
            memset(buf, '\0', sizeof(buf));
            if ((recvLen = recv(sockfd, buf, 500, 0)) == -1) {
                perror("recv");
                std::cout << "Timeout expires"<<std::endl;
                close(sockfd);
                exit(1);
            }
            if(recvLen == 0)
                break;
            for (int j = 0; j < recvLen; j++) {
                payload.push_back(buf[j]);
            }
        }
    }
    else
    {
        for (int i = 0; i < p_size - rest; i+=500) {
            memset(buf, '\0', sizeof(buf));
            if ((recvLen = recv(sockfd, buf, 500, 0)) == -1) {
                perror("recv");
                close(sockfd);
                exit(1);
            }
            for (int j = 0; j < recvLen; j++) {
                payload.push_back(buf[j]);
            }
        }
        
    }
    
    
    if(!payload.empty())
    {
        if(std::ifstream (fname))
        {
            std::cerr << "The file already exist"<<std::endl;
        }
        else
        {
            std::ofstream file;
            file.open (fname,std::ofstream::out | std::ofstream::app | std::ofstream::binary );
            file.write(&payload[0], p_size);
            file.close();
        }
    }
    
    if(response.getMap("Connection") == "close")
    {
        close(sockfd);
    }
}



void do_pipe(std::vector <std::string> urls)
{
    std::vector<std::string> fnames;
    char buf[500] = {0};
    std::vector<char> wire;
    for (int current = 1; current<= urls.size(); current++){
        std::string url = urls[current-1];
        char hostname[500];
        int i = 0;
        while(url[i]!= '/')
            i++;
        if (url[i+1] == '/')
            i+=2;
        else
        {
            std::cerr<<"The first '/' in url is not followed by another '/', thus the url is invalid"<<std::endl;
            exit(1);
        }
        int j = 0;
        while(url[i]!='/' && url[i]!=':'&& i < url.length())
        {
            hostname[j] = url[i];
            i++;
            j++;
        }
        hostname[j] = '\0';
        j = 0;
        int portno = -1;
        if(url[i] == ':')
        {
            char ptno[10];
            i++;
            while(url[i]!= '/')
            {
                ptno[j] = url[i];
                i++;
                j++;
            }
            ptno[j] = '\0';
            portno = atoi(ptno);
        }
        if (portno == -1 )
            portno = 4000;
        i++;
        j=0;
        
        char fname[100] = {'\0'};
        while(url[i] != '\0')
        {
            fname[j] = url[i];
            j++;
            i++;
        }
        std::string filename = fname;
        fnames.push_back(filename);
        
        if((persistent_flag==0) || (persistent_flag==1 && current == 1))
            do_connect(hostname, portno);
        HttpRequest request;
        request.setUrl(url);

        HttpMethod method;
        method.m_methodstr = "GET";
        HttpVersion version;
        if(persistent_flag)
        {
            if(current == urls.size())
                request.setHeader("Connection", "close");
            else
                request.setHeader("Connection", "keep-alive");
            version.m_versionstr = "HTTP/1.1";
        }
        else
        {
            request.setHeader("Connection", "close");
            version.m_versionstr = "HTTP/1.0";
        }
        request.setVersion(version);
        request.setMethod(method);
        wire = request.encode();
        memset(buf, '\0', sizeof(buf));
        
        for (int k =0; k < wire.size(); k+=500){
            memset(buf, '\0', sizeof(buf));
            //fill the buf with things in wire
            for (int k1 = 0; k1 < 500; k1++)
            {
                buf[k1] = wire[k1+k*500];
            }
            
            if (send(sockfd, buf, 500, 0) == -1) {
                perror("send");
                exit(4);
            }
        }

        wire.clear();
    }
    std::vector <char> rec_wire; //cumulate the entire rec_messages
   // std::vector <char> payload; //cumulate the entire payload
    
    
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
    int recvLen = 0;
    while(1){
        memset(buf, '\0', sizeof(buf));
        // receive the buffer
        if ((recvLen = recv(sockfd, buf, sizeof(buf), 0)) == -1) {
            std::cout << "Timeout expired" << std::endl;
            perror("recv");
            exit(1);
        }
        else if(recvLen == 0)
            break;
        for (int i =0; i < recvLen; i++) {
            rec_wire.push_back(buf[i]);
        }
        
    }
    
    std::vector <char> m_helper;
    int count = 0;
    for (int current = 1; current<= urls.size(); current++){
        while (1)
        {
            if(rec_wire[count] == '\r' && rec_wire[count+1] == '\n' && rec_wire[count+2] == '\r' && rec_wire[count+3] == '\n')
            {
                m_helper.push_back(rec_wire[count]);
                m_helper.push_back(rec_wire[count+1]);
                m_helper.push_back(rec_wire[count+2]);
                m_helper.push_back(rec_wire[count+3]);
                break;
            }
            else
            {
                m_helper.push_back(rec_wire[count]);
                count++;
            }
        }
        
        while (1) {
            if(m_helper[0]=='\0')
            {
                m_helper.erase(m_helper.begin());
            }
            else
                break;
        }
        
        //helpe should contain the entire response message
        std::cout << "Received the response message: "<<std::endl;
        for (int i = 0; i < m_helper.size(); i++) {
            std::cout << m_helper[i];
        }
        std::cout << std::endl;
        count+=4;
        HttpResponse response;
        response.decodeFirstLine(m_helper);
        response.decodeHeaderline(m_helper);
        int p_size = stoi(response.getMap("Content-Length"));
        m_helper.clear();
        if(p_size != 0)
        {
            // store payload into m_helper
            for (int i =0; i < p_size; i++)
            {
                m_helper.push_back(rec_wire[count]);
                count++;
            }
            
            std::cout << std::endl;
            
            if(std::ifstream (fnames[current-1]))
            {
                std::cerr << "File already exist" <<std::endl;
                m_helper.clear();
            }
            else
            {
                std::ofstream file;
                file.open (fnames[current-1],std::ofstream::out | std::ofstream::app | std::ofstream::binary );
                file.write(&m_helper[0], p_size);
                file.close();
                m_helper.clear();
            }
        }

        if(response.getMap("Connection") == "close")
            close(sockfd);
    }
    rec_wire.clear();
    
}


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Invalid number of arguments" << std::endl;
        return -1;
    }
    
    
    int option = 0;
    int i = 0;
    while (option!= -1) {
        option = getopt_long(argc, argv, "", long_options, NULL);
        i++;
    }

    int total = argc - i;
    std::cout << "Number of URL is " << total <<std::endl;
    
    int current = 1;
    if(!pipelining_flag)
    {
        for (int j = i; j< argc; j++)
        {
            std::string url = argv[j];
            doHttp(url, current, total);
            current++;
        }
    }
    else if (pipelining_flag && !persistent_flag)
    {
        std::cerr << "Cannot set --pipelining without --persistent" << std::endl;
        return 1;
    }
    else
    {
        std::vector<std::string> all_url;
        for (int j = i; j< argc; j++)
        {
            std::string url = argv[j];
            all_url.push_back(url);
        }
        do_pipe(all_url);
    }
    
    return 0;
}

