#include <string>
#include <thread>
#include <iostream>
#include <sstream>
#include <fstream>
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
#define FINACK 0x3
#define RECV_WINDOW 30720
#define MAX_SEQ_NO 30720
#define HALF_WAY_POINT 15360
#define MAX_PACKET_SIZE 1024
#define SENDSYN 32
#define SYNACK_RECEIVED 33
#define FIN_RECEIVED 34

using namespace std;


struct received_packet
{
    int m_seq;
    char payload[MAX_PACKET_SIZE];
};

class ReceivingBuffer
{
    public:
        void setInitSeq(uint16_t seqNo)
        {
            ini_seq = seqNo;
            round_counter = 0;
            new_round = false;
        }
        uint16_t insert(uint16_t seqNo, HeaderPacket packet)
        {
            received_packet new_packet;
            memset(new_packet.payload, '\0', sizeof(new_packet.payload));
            strcpy(new_packet.payload, packet.payload);
            uint16_t pkt_size = strlen(packet.payload);
            if(pkt_size >= MAX_PACKET_SIZE)
                pkt_size = MAX_PACKET_SIZE;
            
            if(m_segments.empty())
            {
                new_packet.m_seq = seqNo;
                m_segments.push_back(new_packet);
                if(seqNo == ini_seq+1)
                {
                    if(strlen(new_packet.payload) < 1024)
                        return seqNo + strlen(packet.payload);
                    else
                        return seqNo + 1024;
                }
                else
                    return ini_seq + 1;
            }
            
            int seq_32 = seqNo;
            int cumuseq;
            if(seq_32 + pkt_size >= MAX_SEQ_NO && (new_round == false))
            {
                new_round = true;
                cumuseq = seq_32 + round_counter;
                round_counter += MAX_SEQ_NO;
            }
            else
            {
                if(new_round)
                {
                    if(seq_32 < HALF_WAY_POINT)
                        cumuseq = seq_32 + round_counter;
                    else
                        cumuseq = seq_32 + round_counter - MAX_SEQ_NO;
                }
                else
                {
                    if(round_counter == 0)
                    {
                        cumuseq = seq_32;
                    }
                    else
                    {
                        if(seq_32 < HALF_WAY_POINT)
                            cumuseq = seq_32 + round_counter + MAX_SEQ_NO;
                        else
                            cumuseq = seq_32 + round_counter;
                    }
                }
            }
            new_packet.m_seq = cumuseq;
            
            vector<received_packet>::iterator it = m_segments.begin();
            int i = 0;
            while (i < m_segments.size())
            {
                if(m_segments[i].m_seq > cumuseq)
                {
                    m_segments.insert(it+i, new_packet);
                    cout << "Should insert here with packet " << cumuseq <<"at position " << i <<endl;
                    break;
                }
                else if(m_segments[i].m_seq == cumuseq)
                {
                    uint16_t segsize = strlen(m_segments[i].payload);
                    if(segsize >= 1024)
                        segsize = 1024;
                    return (m_segments[i].m_seq+segsize) % MAX_SEQ_NO;
                }
                
                i++;
            }
            if(i == m_segments.size())
            {
                cout << "PUSH THE PACKET TO THE END" <<endl;
                m_segments.push_back(new_packet);
            }
            
            int j;
            uint16_t segsize;
            for (j = 0; j < m_segments.size()-1; j++) {
                segsize = strlen(m_segments[j].payload);
                if(segsize >= 1024)
                    segsize = 1024;
                if((m_segments[j].m_seq+segsize) != m_segments[j+1].m_seq)
                    break;
            }
            if((m_segments[j].m_seq % MAX_SEQ_NO < HALF_WAY_POINT) && new_round == true)
                new_round = false;
            
            segsize = strlen(m_segments[j].payload);
            if(segsize >= 1024)
                segsize = 1024;
            return (m_segments[j].m_seq + segsize) % MAX_SEQ_NO;
        }
        vector<received_packet> getSegments()
        {
            return m_segments;
        }
    private:
    vector<received_packet> m_segments;
    bool new_round;
    int round_counter;
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

    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4000);     // short, network byte order
    addr.sin_addr.s_addr = inet_addr("10.0.0.2");
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
    
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 2;
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
    int serlen = sizeof(servaddr);
    
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
   // uint16_t next_seq;
    //bool finish = false;
    HeaderPacket req_pkt;
    req_pkt.m_seq = htons(0);
    req_pkt.m_ack = htons(0);
    req_pkt.m_flags = htons(0x4);
    req_pkt.m_window = htons(RECV_WINDOW);
    if(sendto(sockfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr, serlen) <0)
    {
        cerr<<"Sendto fails" <<endl;
        return 1;
    }
    FD_SET(sockfd, &readFds);
    int state = SENDSYN;
    while (true) {
        int nReadyFds = 0;
        if(state == SENDSYN)
        {
            tv.tv_sec = 0;
            tv.tv_usec = 500000;
        }
        else if (state == SYNACK_RECEIVED)
        {
            tv.tv_sec = 30;
            tv.tv_usec = 0;
        }
        else if(state == FIN_RECEIVED)
        {
            tv.tv_sec = 3;
            tv.tv_usec = 0;
        }
        if(!FD_ISSET(sockfd, &watchFds))
            FD_SET(sockfd, &watchFds);
        errFds = watchFds;
        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, &tv)) == -1) {
            perror("select");
            return 4;
        }
        if(nReadyFds == 0)
        {
            cout << "TIMEOUT HAPPENS TO THE CLIENT" <<endl;
            if(state == FIN_RECEIVED || state ==SYNACK_RECEIVED)
            {
                FD_CLR(sockfd, &readFds);
                close(sockfd);
                break;
            }
            if (state == SENDSYN)
            {
                HeaderPacket req_pkt;
                req_pkt.m_seq = htons(genRanfom());
                req_pkt.m_ack = htons(0);
                req_pkt.m_flags = htons(0x4);
                req_pkt.m_window = htons(RECV_WINDOW);
                if(sendto(sockfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr, serlen) <0)
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
                    if(recvfrom(fd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr*) &servaddr, (socklen_t*)&serlen) <0 )
                    {
                        cerr<<"Failure in recvfrom" <<endl;
                        return 1;
                    }
                    cout << "Receiving data packet " << ntohs(res_pkt.m_seq)<<endl;
                    uint16_t flagcheck = ntohs(res_pkt.m_flags);
                    switch (flagcheck) {
                        case SYNACK:{
                            next_ack = ntohs(res_pkt.m_seq)+1;
                            recv_buffer.setInitSeq(ntohs(res_pkt.m_seq));
                            writestate = SYNACK;
                            break;
                        }
                        case PACKET:{
                            //next_seq = ntohs(res_pkt.m_ack);
                            next_ack = recv_buffer.insert(ntohs(res_pkt.m_seq), res_pkt);
                            writestate = PACKET;
                            break;
                        }
                        case FIN:{
                            //next_seq = ntohs(res_pkt.m_ack);
                            next_ack = ntohs(res_pkt.m_seq);
                            writestate = FIN;
                            break;
                        }
                        default:
                        {
                            cerr<< "we get a packet that has invalid flag" <<endl;
                            return 1;
                        }
                    }
                    FD_CLR(fd, &readFds);
                    FD_SET(fd, &writeFds);
                    
                }
                else if(FD_ISSET(fd, &writeFds))
                {
                    switch (writestate) {
                        case SYNACK:{
                            HeaderPacket req_pkt;
                            memset(req_pkt.payload, '\0', sizeof(req_pkt.payload));
                            req_pkt.m_seq = htons(0x0);
                            req_pkt.m_ack = htons(next_ack);
                            req_pkt.m_flags = htons(SYNACK);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            cout <<"Sending ACK packet " << ntohs(req_pkt.m_ack) <<endl;
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr, serlen)<0)
                            {
                                cerr<<"Sendto fails" <<endl;
                                return 1;
                            }
                            state = SYNACK_RECEIVED;
                            break;
                        }
                        case PACKET:{
                            HeaderPacket req_pkt;
                            req_pkt.m_seq = htons(0x0);
                            memset(req_pkt.payload, '\0', sizeof(req_pkt.payload));
                            cout <<"sending acks to packets" <<endl;
                            req_pkt.m_ack = htons(next_ack);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            req_pkt.m_flags = htons(0x2);
                            cout <<"Sending ACK packet " << ntohs(req_pkt.m_ack) <<endl;
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr, serlen)<0)
                            {
                                cerr<<"Sendto fails" <<endl;
                                return 1;
                            }
                            break;
                        }
                        case FIN: {
                            //req_pkt.m_seq = htons(next_seq);
                            HeaderPacket req_pkt;
                            memset(req_pkt.payload, '\0', sizeof(req_pkt.payload));
                            cout <<"sending ack to fin" <<endl;
                            req_pkt.m_seq = htons(0x0);
                            req_pkt.m_ack = htons(next_ack);
                            req_pkt.m_window = htons(RECV_WINDOW);
                            req_pkt.m_flags = htons(FINACK);
                            //finish = true;
                            state = FIN_RECEIVED;
                            cout <<"Sending ACK packet " << ntohs(req_pkt.m_ack) <<endl;
                            if(sendto(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr *)&servaddr, serlen)<0)
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
                    }
                        FD_CLR(fd, &writeFds);
                        FD_SET(fd, &readFds);
                    
                }
            }
            
        }
        
    }
    //we gonna write into the file
    cout << "write into a file" <<endl;
    if(ifstream("received.data"))
    {
        cout <<"received.data already exists" <<endl;
    }
    else
    {
        ofstream file;
        file.open ("received.data",ofstream::out | ofstream::app | ofstream::binary );
        vector<received_packet> segment = recv_buffer.getSegments();
        //print out content
        for (int i = 0; i < segment.size(); i++) {
            uint16_t write_size = strlen(segment[i].payload);
            if (write_size >= 1024) {
                write_size = 1024;
            }
            file.write(&segment[i].payload[0], write_size);
        }
        file.close();
    }
}

