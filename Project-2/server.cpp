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
#include <vector>
#include <sstream>
#include <fstream>
#include "packet.h"

//defining flags and useful numbers
#define SYN 0x4;
#define ACK 0x2;
#define FIN 0x1;
#define SYNACK 0x6;
#define FINACK 0x3;

// defining states and corresponding numbers
#define NO_CONNECTION 30;
#define SYNACK_SENT 31;
#define CONNECTED 32;
#define SENT_FIN 33;

using namespace std;


// random number generator: used to generate initial sequence number
uint16_t genRandom()
{
   return rand()%50;
}

class FileReader
{
public:
    HeaderPacket top();
    void pop();
    bool hasMore();
private:
    
};

class OutputBuffer
{
public:
    void setup()
    {
        cong_window_size = 1;
        pkts_in_window = 0;
        window_begin = 0;
        window_end = 0;
    }
    void setInitSeq(uint16_t seqNo)
    {
        seqNo_s = seqNo;
    }
    void ack(uint16_t ackNo)
    {
        // Ideally, I think it would be good check to see if the ACK number matches with what is expected
        // However, that is a little hard to implement
        // Instead, we will assume the client does not send us bad ACK numbers
        // So, we will just set our current sequence number to the ACK number
        seqNo_s = ackNo;
    }
    void timeout();
    bool hasSpace(uint16_t size = 1024)
    {
        return (pkts_in_window < cong_window_size);
    }
    void insert(HeaderPacket pkt)
    {
        m_packets_s.push_back(pkt);
        window_end++;
    }
    bool hasNext()
    {
        return (window_begin != window_end);
    }
    HeaderPacket getPkt(uint16_t seqNo)
    {
        if (m_packets_s[window_begin].m_seq == seqNo)
        {
            return m_packets_s[window_begin];
        }
        return nullptr;
    }
    uint16_t getNextSeqNo()
    {
        // return sequence number for next packet, which should be updated correctly each time
        return seqNo_s;
    }
private:
    vector<HeaderPacket> m_packets_s;
    uint16_t seqNo_s;
    uint16_t cong_window_size; // in number of packets
    uint16_t pkts_in_window; // in number of packets
    // These next two are needed because I am not deleting packets from our m_packets_s vector once they are ACK'ed
    // I am simply incrementing an index to adjust the window to the currently sending window
    uint16_t window_begin; // index of current beginning of window
    uint16_t window_end; // index of current end of window
};

int main(int argc, char* argv[])
{
    // Check for correct number of arguments
    if(argc != 3)
    {
        cerr << "Invalid number of arguments"<<std::endl;
        return 1;
    }
    
    // set up UDP socket, including BINDING hints, IP, and port number
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Can't create socket");
        exit(-1);
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
    addr.sin_addr.s_addr = inet_addr("10.0.0.1");
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
    
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 2;
    }
    
    // handle initial SYN request
    // placed this outside event loop because it would be hard to start up
        // the server and the client within the 500ms timeout window otherwise
    HeaderPacket req_pkt_syn;
    int clilen = 0;
    struct sockaddr_in clientAddr;
    OutputBuffer out_buf;
    uint16_t state = NO_CONNECTION;
    
    if(recvfrom(sockfd, &req_pkt_syn, sizeof(req_pkt_syn), 0, (struct sockaddr*) &clientAddr, (socklen_t*) &clilen) < 0)
    {
        cerr << "recvfrom failed" << endl;
        return 1;
    }
    else if (req_pkt_syn)
    {
        state = SYNACK_SENT;
        uint16_t initseq = genRandom();
        out_buf.setInitSeq(initseq);
        
        // sending out SYNACK packet
        HeaderPacket resp_pkt_synack;
        resp_pkt_synack.m_seq = htons(out_buf.getNextSeqNo());
        resp_pkt_synack.m_ack = 0;
        resp_pkt_synack.m_flags = htons(0x6); //SYNACK flag
        resp_pkt_synack.m_window = htons(RECV_WINDOW);
        if(sendto(sockfd, &resp_pkt_synack, sizeof(resp_pkt_synack), 0, (struct sockaddr*) &clientAddr, (socklen_t*) &clilen)<0)
        {
            cerr << "Sendto fails" << endl;
            return 1;
        }
    }

    
    
    // getting ready to receive from client, set up event loop
    // setting up sets for select() call
    fd_set readFds;
    fd_set writeFds;
    fd_set errFds;
    fd_set watchFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errFds);
    FD_ZERO(&watchFds);

    
    
    while (true) { // event loop
        // setting up select() call
        int nReadyFds = 0;
        errFds = watchFds;
        if(!FD_ISSET(sockfd, &watchFds))
            FD_SET(sockfd, &watchFds);
        if(!FD_ISSET(sockfd, &readFds))
            FD_SET(sockfd, &readFds);
        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, NULL)) == -1) {
            perror("select");
            return 4;
        }
        
        
        if (nReadyFds == 0) { // CASE TIMEOUT
            std::cout << "no data is received" << std::endl;
            //handle the timeout
            break;
        }
        else
        {
            for(int fd = 0; fd <= maxSockfd; fd++){
                if (FD_ISSET(fd, &readFds)) // reading, recvfrom cases
                {
                    cout << "Read something "<< endl;
                    HeaderPacket req_pkt;
                    if(recvfrom(fd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &clientAddr, (socklen_t*) &clilen) < 0)
                    {
                        cerr << "recvfrom failed" << endl;
                        return 1;
                    }
                    else
                    {
                        switch(req_pkt.m_flags)
                        {
                            
                            case ACK:
                            {
                                
                            }
                            default:
                        }
                    }
                    cout<<"The packet has a sequence number "<<req_pkt.m_seq <<endl;
                    cout<<"The packet has flag " << req_pkt.m_flags << endl;
                    
                    FD_CLR(fd, &readFDs);
                    FD_SET(fd, &writeFDs);
                }
                
                else if(FD_ISSET(fd, &writeFDs) // writing, sendto cases
                {
                    HeaderPacket res_pkt;
                    res_pkt.m_window = req_pkt.m_window;
                    res_pkt.m_ack = req_pkt.m_seq + 1;
                    if(req_pkt.m_ack == 0)
                    {
                        res_pkt.m_seq = genRandom();
                        res_pkt.m_flags |= 0x4;
                        res_pkt.m_flags |= 0x2;
                    }
                    else
                    {
                        res_pkt.m_seq = req_pkt.m_ack;
                        if(startfin)
                        {
                            res_pkt.m_flags|=0x1;
                        }
                        else
                        {
                            res_pkt.m_flags |= 0x2;
                            ifstream file;
                            file.exception(ifstream::badbit | ifstream::eofbit);
                            file.open(argv[2], ifstream::in| ifstream::binary | ifstream::out);
                            if(!file.is_open())
                            {
                                cerr<<"File does not exist"<<endl;
                                return 1;
                            }
                            else
                            {
                                file.seekg(0, ios::end);
                                streampos length(file.tellg());
                                if(length){
                                    file.seekg(0, ios::beg);
                                    file.read(res_pkt.payload, static_cast<size_t>(length));
                                }
                                file.close();
                            }
                        }
                        if(sendto(fd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr *)&clientAddr, (socklen_t*)clilen)< 0)
                        {
                            cerr << "sendto failed" << endl;
                            return 1;
                        }

                    }
		   
                } // closing brace for elseif writing, sendto cases
            } // closing brace for for loop, non-timeout cases
        } // closing brace for else, non-timeout cases
    } //closing brace for while loop, event loop
    
} // closing brace for main function
