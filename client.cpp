#include <string>
#include <thread>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include "packet.h"


#define SYNACK 0x6
#define PACKET 0x0
#define FIN 0x1
#define RECV_WINDOW 30720

using namespace std;




class ReceivingBuffer
{
    public:
        void setInitSeq(uint16_t seqNo)
        {
            ini_seq = seqNo;
        }
        uint16_t insert(uint16_t seqNo, HeaderPacket packet)
        {
            
            if(m_segments.empty())
            {
                m_segments.push_back(packet);
                if(seqNo == ini_seq+1)
                    return seqNo + strlen(packet.payload);
                else
                    return ini_seq + 1;
            }
            

            vector<HeaderPacket>::iterator it = m_segments.begin();
            int i = 0;
            uint16_t return_val;
            while (i < m_segments.size())
            {
                if(m_segments[i].m_seq > seqNo)
                {
                    m_segments.insert(it+i, seqNo);
                    break;
                }
                i++;
            }
            if(i == m_segments.size())
                m_segments.push_back(packet);
            int j;
            for (j = 0; j < m_segments.size()-1; j++) {
                if((m_segments[j].m_seq+strlen(m_segments[j].payload)) != m_segments[j+1].m_seq)
                    return m_segments[j].m_seq+strlen(m_segments[j].payload);
            }
            return m_segments[j].m_seq + strlen(m_segments[j].payload());
        }
        uint16_t get_ini_seq()
        {
            return ini_seq;
        }
    private:
    vector<HeaderPacket> m_segments;
    int ini_seq;
};



uint16_t genRanfom()
{
    return rand() % 50;
}



int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Invalid number of arguments" << std::endl;
        return -1;
    }
    
    int portno = atoi(argv[2]);
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Can't create socket");
        return 1;
    }

    struct addrinfo hints;
    struct addrinfo* res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    int status = 0;
    cout <<"Print out host address" <<endl;
    if((status=getaddrinfo(argv[1], "80", &hints, &res))!=0)
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
        ntohs(ipv4->sin_port);
    }
    
    struct sockaddr_in servaddr;
    memset((char*)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno);
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));
    
    //setting up the sets for the sockets
    fd_set readFds;
    fd_set writeFds;
    fd_set errFds;
    fd_set watchFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errFds);
    FD_ZERO(&watchFds);
    int maxSockfd = sockfd;
    ReceivingBuffer recv_buffer;
    struct timeval tv;
    uint16_t writestate;
    uint16_t next_ack;
    uint16_t next_seq;
    bool finish = false;
    while (true) {
        int nReadyFds = 0;
        if(!finish)
        {
            tv.tv_sec = 0;
            tv.tv_usec = 500000;
        }
        else
        {
            tv.tv_sec = 2;
            tv.tv_usec = 0;
        }
        if(!FD_ISSET(sockfd, &watchFds))
            FD_SET(sockfd, &watchFds);
        errFds = watchFds;
        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, NULL)) == -1) {
            perror("select");
            return 4;
        }
        if(nReadyFds == 0)
        {
            if(finish)
            {
                FD_CLR(&sockfd, &readFds);
                close(fd);
                break;
            }
            else {
                HeaderPacket req_pkt;
                req_pkt.m_seq = htons(genRanfom());
                req_pkt.m_ack = htons(0);
                req_pkt.m_flags = htons(0x4);
                req_pkt.m_window = htons(RECV_WINDOW);
                if(sendto(sockfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr)<0)
                {
                    cerr<<"Sendto fails" <<endl;
                    return 1;
                }
            }
        }
        else
        {
            for(int fd = 0; fd <= maxSockfd; fd++)
            {
                if(FD_ISSET(fd, &readFds))
                {
                    HeaderPacket res_pkt;
                    int serlen;
                    if(recvfrom(fd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr*) &servaddr, (socklen_t*)&serlen) <0 )
                    {
                        cerr<<"Failure in recvfrom" <<endl;
                        return 1;
                    }
                    uint16_t flagcheck = ntohs(res_pkt.m_flags);
                    switch (flagcheck) {
                        case SYNACK:{
                            recv_buffer.setInitSeq(ntohs(res_pkt.m_seq));
                            writestate = SYNACK;
                            break;
                        }
                        case PACKET:{
                            next_seq = ntohs(res_pkt.m_ack);
                            next_ack = recv_buffer.insert(ntohs(res_pkt.m_seq));
                            writestate = PACKET;
                            break;
                        }
                        case FIN:{
                            next_seq = ntohs(res_pkt.m_ack);
                            next_ack = ntohs(res_pkt.m_seq) + 1;
                            writestate = FIN;
                            break;
                        }
                        default:
                        {
                            cerr<< "we get a packet that has invalid flag";
                            return 1;
                        }
                    }
                    FD_CLR(&fd, &readFds);
                    FD_SET(&fd, &writeFds);
                    
                }
                else if(FD_ISSET(fd, &writeFds))
                {
                    HeaderPacket req_pkt;
                    switch (writestate) {
                        case SYNACK:{
                            req_pkt.m_seq = htons(genRanfom());
                            req_pkt.m_ack = recv_buffer.get_ini_seq() + 1;
                            req_pkt.m_flags = htons(0x2);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr)<0)
                            {
                                cerr<<"Sendto fails" <<endl;
                                return 1;
                            }
                            
                            break;
                        }
                        case PACKET:{
                            req_pkt.m_seq = htons(next_seq);
                            req_pkt.m_ack = htons(next_ack);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            req_pkt.m_flags = htons(0x2);
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr)<0)
                            {
                                cerr<<"Sendto fails" <<endl;
                                return 1;
                            }
                            break;
                        }
                        case FIN: {
                            req_pkt.m_seq = htons(next_seq);
                            req_pkt.m_ack = htons(next_ack);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            req_pkt.m_flags = htons(0x3);
                            finish = true;
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr)<0)
                            {
                                cerr<<"Sendto fails" <<endl;
                                return 1;
                            }
                            break;
                        }
                        default:
                        {
                            cerr<<"This flag is invalid" <<endl;
                            return 1;
                        }
                        FD_CLR(&fd, &wirteFds);
                        FD_SET(&fd, &readFds);
                    }
                    
                }
            }
            
        }
        
    }
    //we gonna write into the file
    std::ofstream file;
    file.open ("received.data",std::ofstream::out | std::ofstream::app | std::ofstream::binary );
    for (int i = 0; i < recv_buffer.size(); i++) {
        file.write(&recv_buffer[i].payload[0], strlen(recv_buffer[i].payload));
    }
    file.close();
}

