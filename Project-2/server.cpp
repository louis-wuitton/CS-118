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
#define MAX_NO_TIMEOUTS 3;
#define INITIAL_SLOW_START_THRESHOLD 30720;
#define MAX_PACKET_SIZE 1024;

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
    void openfile(char* name);
    HeaderPacket top(char* );
    void pop(int index);
    bool hasMore();
private:
    FILE* pfile
    long m_position;
    long f_size;
};

void FileReader::openfile(char* name)
{
    pfile = fopen(name, "r");
    if(pfile == NULL)
    {
        cerr << "Fail to open the file" <<endl;
    }
    fseek(pFile, 0, SEEK_END);
    f_size = ftell(pFile);
    rewind(pFile);
}


HeaderPacket FileReader::top()
{
    HeaderPacket segment;
    int readsize;
    if(f_size - m_position < 1024)
        readsize = m_position % 1024;
    else
        readsize = 1024;
    
    if (!feof(pFile)) {
        fseek(pFile, m_position*(sizeof(char)),SEEK_SET);
        fread(segment.payload, sizeof(char), readsize, pFile);
    }
    m_position += readsize;
    return segment;
}

void FileReader::pop()
{
    m_position += 1024;
}

bool FileReader::hasMore()
{
    if(m_position >= lsize)
        return false;
    else
        return true;
}

class OutputBuffer
{
public:
    void setup()
    {
        cong_window_size = 1024;
        cur_cong_window = 0;
        window_begin = 0;
        window_end = 0;
        no_timeouts = 0;
        slow_start_threshold = INITIAL_SLOW_START_THRESHOLD;
    }
    void setInitSeq(uint16_t seqNo)
    {
        seqNo_s = seqNo;
    }
    void ack(uint16_t ackNo)
    {
        // Check to see if the ACK number matches with what is expected
        if ((m_packets_s[window_begin].m_seq + strlen(m_packets_s[window_begin].payload)) == ackNo)
        {
            // set our current sequence number to the ACK number
            seqNo_s = ackNo;
            // also have to reset timeout counter
            no_timeouts = 0;
            // also have to reduce current congestion window
            cur_cong_window -= strlen(m_packets_s[window_begin].payload);
            window_begin++;
            
            // adjust congestion window size accordingly
            // slow start case
            if (cong_window_size <= slow_start_threshold)
            {
                cong_window_size += 1024;
            }
            else // congestion avoidance case
            {
                cong_window_size += (double) ((1/(cong_window_size/1024)) * MAX_PACKET_SIZE);
            }
        }
    }
    bool timeout()
    {
        // set slow start threshold to half of current congestion window
        slow_start_threshold = 0.5 * cong_window_size;
        // set congestion window back to 1 packet = 1024 bytes
        cong_window_size = 1024;
        // also need to reset current congestion window size: restart sending from packet that timed out
        cur_cong_window = 0;
        window_end = window_begin; // part of restarting send from packet that timed out
        
        no_timeouts++;
        return no_timeouts == MAX_NO_TIMEOUTS;
    }
    bool hasSpace(uint16_t size = 1024)
    {
        return ((cur_cong_window + size) <= cong_window_size);
    }
    void insert(HeaderPacket pkt)
    {
        m_packets_s.push_back(pkt);
        window_end++;
        cur_cong_window += strlen(pkt.payload);
    }
    bool hasNext()
    {
        return (window_begin != window_end) && (cur_cong_window != 0);
    }
    HeaderPacket getPkt(uint16_t seqNo)
    {
        for (uint16_t i = window_begin; i <= window_end; i++)
        {
            if (m_packets_s[i].m_seq == seqNo)
            {
                return m_packets_s[i];
            }
        }
        return nullptr;
    }
    uint16_t getNextSeqNo()
    {
        // return sequence number for next packet, which should be updated correctly each time
        return seqNo_s;
    }
    void setCongWindSize(uint16_t window_size)
    {
        cong_window_size = window_size;
    }
private:
    vector<HeaderPacket> m_packets_s;
    uint16_t seqNo_s;
    double cong_window_size; // in number of bytes
    uint16_t cur_cong_window; // number of bytes currently in congestion window
    // These next two are needed because I am not deleting packets from our m_packets_s vector once they are ACK'ed
    // I am simply incrementing an index to adjust the window to the currently sending window
    uint16_t window_begin; // index of current beginning of window
    uint16_t window_end; // index of current end of window
    uint16_t no_timeouts; // number of timeouts
    double slow_start_threshold; // slow start threshold: changes depending on when timeouts occur
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
                                // check the state
                                if (state == SYNACK_SENT)
                                {
                                	state = CONNECTED;
                                }
                                else if (state == SENT_FIN)
                                {
                                	
                                }
                                else if (state == CONNECTED)
                                {
                                	
                                }
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
