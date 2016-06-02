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
#include <ctime>
#include <cmath>
#include <math.h>
#include "packet.h"


//this one implement advanced version

//defining flags and useful numbers
#define SYN 0x4
#define ACK 0x2
#define FIN 0x1
#define SYNACK 0x6
#define FINACK 0x3
#define MAX_NO_TIMEOUTS 3

//#define MAX_PACKET_SIZE 1024


// defining states and corresponding numbers
#define NO_CONNECTION 30
#define SYNACK_SENT 31
#define CONNECTED 32
#define SENT_FIN 33

#define MAX_SEQ_NO 30720
#define MAX_CONGESTION_WINDOW 15360

using namespace std;


struct exe_time
{
    clock_t begin;
};


// random number generator: used to generate initial sequence number
uint16_t genRandom()
{
    return rand()%50;
}

class FileReader
{
public:
    void openfile(char* name);
    HeaderPacket top();
    void pop();
    bool hasMore();
private:
    char m_name[100] = {'\0'};
    char* filecontent;
    long m_position;
    long f_size;
};

void FileReader::openfile(char* name)
{
    ifstream m_file;
    m_file.open(name, ifstream::in | ifstream::binary | ifstream::out);
    if(!m_file.is_open())
    {
        cerr << "Fail to open the file" <<endl;
    }
    //fseek(pfile, 0, SEEK_END);
    strcpy(m_name, name);
    m_file.seekg (0, m_file.end);
    m_position = 0;
    f_size = m_file.tellg();
    cout << "file size is "<<f_size <<endl;
    m_file.seekg(0, m_file.beg);
    //cout << "filesize now is "<<f_size <<endl;
    
    filecontent = new char[f_size+1];
    
    m_file.read(filecontent, f_size);
    
    m_file.close();
}


HeaderPacket FileReader::top()
{
    HeaderPacket segment;
   // m_file.seekg(m_position, m_file.beg);
    memset(segment.payload, '\0', sizeof(segment.payload));
    int readsize;
    if(f_size - m_position < 1024)
        readsize = f_size % 1024;
    else
        readsize = 1024;
    
    cout << "right now the position is "<<m_position <<endl;

    memcpy(segment.payload, &filecontent[m_position],readsize);
    //m_position += readsize;
    cout << "length is " << strlen(segment.payload) << endl;

    /*
    cout << "print out payload" <<endl;
    for(int i = 0; i < readsize; i++)
    {
        cout << segment.payload[i];
    }
    cout << endl;
     */
    return segment;
}

void FileReader::pop()
{
    m_position += 1024;
}

bool FileReader::hasMore()
{
    
    if(m_position >= f_size)
    {
        cout << "POSITION IS CURRENTLY :" << m_position <<endl;
        return false;
    }
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
        window_pointer = window_begin;
        no_timeouts = 0;
        slow_start_threshold = 30720;
        repeat_count = 0;
    }
    void setInitSeq(uint16_t seqNo)
    {
        seqNo_s = seqNo;
    }
    void ack(uint16_t ackNo)
    {
         //cout <<"Current congestion window size is "<<cong_window_size <<endl;
        if(ntohs(m_packets_s[window_begin].m_seq) == ackNo)
        {
            repeat_count++;
        }
        else
        {
            repeat_count = 0;
        }
        
        for (int i = window_begin; i <= window_pointer; i++)
        {
            uint16_t i_length = strlen(m_packets_s[i].payload);
            if (i_length >= 1024)
                i_length = 1024;
            
            cout << "right now i_length is " << i_length <<endl;
            if (((ntohs(m_packets_s[i].m_seq) + i_length) % MAX_SEQ_NO) == ackNo)
            {
                cout << "IF STATEMENT PASS " <<endl;
                seqNo_s = ackNo;
                no_timeouts = 0;
                
                if(i == window_begin)
                {
                    window_begin++;
                
                    cur_cong_window -= i_length;
                    if (cong_window_size <= slow_start_threshold)
                    {
                        cong_window_size += 1024;
                        if(cong_window_size > MAX_CONGESTION_WINDOW)
                            cong_window_size = MAX_CONGESTION_WINDOW;
                        cout <<"Updated congestion window size is "<<cong_window_size <<endl;
                    }
                    else // congestion avoidance case
                    {
                        cong_window_size +=  ((1/(cong_window_size/1024)) * 1024);
                        if(cong_window_size > MAX_CONGESTION_WINDOW)
                            cong_window_size = MAX_CONGESTION_WINDOW;
                        cout <<"Updated congestion window size is "<<cong_window_size <<endl;
                    }
                }
                else if(i <= window_pointer)
                {
                    uint16_t offset = i - window_begin;
                    for (int j = window_begin; j <= i; j++)
                    {
                        uint16_t j_length = strlen(m_packets_s[j].payload);
                        if(j_length >= 1024)
                            j_length = 1024;
                        cur_cong_window -= j_length;
                        
                        if (cong_window_size <= slow_start_threshold)
                        {
                            cong_window_size += 1024;
                             if(cong_window_size > MAX_CONGESTION_WINDOW)
                                cong_window_size = MAX_CONGESTION_WINDOW;
                            cout <<"Updated congestion window size is "<<cong_window_size <<endl;
                        }
                        else // congestion avoidance case
                        {
                            cong_window_size +=  ((1/(cong_window_size/1024)) * 1024);
                            if(cong_window_size > MAX_CONGESTION_WINDOW)
                                cong_window_size = MAX_CONGESTION_WINDOW;
                            cout <<"Updated congestion window size is "<<cong_window_size <<endl;
                        }
                    }
                    window_begin += offset;
                }
            }
        }
    }
    
    bool should_retransmit()
    {
        return repeat_count == 3;
    }

    bool timeout()
    {
        // set slow start threshold to half of current congestion window
        slow_start_threshold = 0.5 * cong_window_size;
        // set congestion window back to 1 packet = 1024 bytes
        cong_window_size = 1024;
        // also need to reset current congestion window size: restart sending from packet that timed out
        cur_cong_window = 0;
        window_end = window_begin;
        window_pointer = window_begin;
        
        
        //seqNo_s
        no_timeouts++;
        return no_timeouts == MAX_NO_TIMEOUTS;
    }
    
    bool hasSpace(uint16_t size = 1024)
    {
        if(size >= 1024)
            size = 1024;
        return ((cur_cong_window + size) <= cong_window_size);
    }
    
    void insert(HeaderPacket pkt)
    {
        //insert the data packet into the buffer
        
        if(!m_packets_s.empty())
        {
            int bufsize = m_packets_s.size();
           // cout << "inserting packet with size " << p_size<<endl;
            uint16_t add_on = strlen(m_packets_s[bufsize-1].payload);
            if(add_on >= 1024){
                add_on = 1024;
            }
            uint16_t new_seq = (ntohs(m_packets_s[bufsize-1].m_seq) + add_on) % MAX_SEQ_NO;
            pkt.m_seq = htons(new_seq);
        }
        else
        {
            pkt.m_seq = htons(seqNo_s);
        }
        
        pkt.m_ack = htons(0x0);
        pkt.m_flags = htons(0x0);
        m_packets_s.push_back(pkt);
        //insert time as well
        exe_time timeslot;
        m_time.push_back(timeslot);
        window_end++;
        cur_cong_window += strlen(pkt.payload);
    }
    
    bool hasNext()
    {
        cout << "window begin is " << window_begin <<endl;
        bool result = (window_begin != window_end) && (cur_cong_window != 0);
        if(result)
        {
            cout << "There is next "<<endl;
        }
        else
        {
            cout << "Has next gives false, should switch to fin now" <<endl;
        }
        return result;
    }
    
    
    HeaderPacket getPkt(uint16_t seqNo)
    {
        for (uint16_t i = window_begin; i <= window_end; i++)
        {
            if (ntohs(m_packets_s[i].m_seq) == seqNo)
            {
                window_pointer++;
                return m_packets_s[i];
            }
        }
        cout << "not finding anything" <<endl;
        
        //return NULL;
    }
    uint16_t retcumulative()
    {
        // return sequence number for next packet, which should be updated correctly each time
        return seqNo_s;
    }
    
    uint16_t getNextSeqNo()
    {
        return ntohs(m_packets_s[window_pointer].m_seq);
    }
    void setCongWindSize(uint16_t window_size)
    {
        cong_window_size = window_size;
    }
    
    uint16_t getWindowSize()
    {
        return cong_window_size;
    }
    
    bool switch_to_read()
    {
        cout << "Window end is "<<window_end << " and window pointer is "<<window_pointer <<endl;
        return window_pointer == window_end;
    }
    
    bool switch_to_write()
    {
        return window_begin == window_pointer;
    }
    void reset_pointer()
    {
        window_pointer = window_begin;
    }
    
    
    void record_start(uint16_t seq, double begin)
    {
        for (int i = window_pointer; i < window_end; i++) {
            if(ntohs(m_packets_s[i].m_seq) == seq)
            {
                m_time[i].begin = begin;
                break;
            }
        }
    }
    double get_duration(uint16_t ack, double end)
    {
        for(int i = window_begin; i<=window_pointer; i++)
        {
            uint16_t i_size = strlen(m_packets_s[i].payload);
            if (i_size >= 1024)
                i_size = 1024;
            if(ntohs(m_packets_s[i].m_seq) + i_size == ack)
            {
                return (end - m_time[i].begin)/(double) CLOCKS_PER_SEC;
            }
        }
    }
    
    
private:
    vector<HeaderPacket> m_packets_s;
    vector<exe_time> m_time;
    uint16_t seqNo_s;
    double cong_window_size; // in number of bytes
    double cur_cong_window; // number of bytes currently in congestion window
    uint16_t window_begin; // index of current beginning of window
    uint16_t window_end; // index of current end of window
    uint16_t window_pointer;
    uint16_t no_timeouts; // number of timeouts
    double slow_start_threshold;
    uint16_t repeat_count;
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
    
    HeaderPacket req_pkt_syn;
    struct sockaddr_in clientAddr;
    socklen_t clilen = sizeof(clientAddr);

    OutputBuffer mybuffer;
    mybuffer.setup();
    uint16_t state = NO_CONNECTION;
    
    if(recvfrom(sockfd, &req_pkt_syn, sizeof(req_pkt_syn), 0, (struct sockaddr*) &clientAddr, (socklen_t*) &clilen) < 0)
    {
        cerr << "recvfrom failed" << endl;
        return 1;
    }
    else
    {
      
        if(ntohs(req_pkt_syn.m_flags) != SYN)
        {
            cerr << "Received a packet that's not SYN, cannot set up connection" <<endl;
            return 1;
        }
        uint16_t initseq = genRandom();
        
        // sending out SYNACK packet
        HeaderPacket resp_pkt_synack;
        resp_pkt_synack.m_seq = htons(initseq);
        resp_pkt_synack.m_ack = htons(0); //server does not need to ack with anything
        resp_pkt_synack.m_flags = htons(SYNACK); //SYNACK flag
        //resp_pkt_synack.m_window = htons(RECV_WINDOW);
        
        if(sendto(sockfd, &resp_pkt_synack, sizeof(resp_pkt_synack), 0, (struct sockaddr*) &clientAddr, clilen)<0)
        {
            cerr << "Sendto fails" << endl;
            return 1;
        }
        state = SYNACK_SENT;
    }
    
    fd_set readFds;
    fd_set writeFds;
    fd_set errFds;
    fd_set watchFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&errFds);
    FD_ZERO(&watchFds);
    
    FileReader myreader;
    myreader.openfile(argv[2]);
    
    bool finish = false;
    
    
    double sRTT = 3;
    double devRtt = 3;

    struct timeval tv;
   
    
    if(!FD_ISSET(sockfd, &readFds))
        FD_SET(sockfd, &readFds);

    while (true) {
        int nReadyFds = 0;
        errFds = watchFds;
        if(!FD_ISSET(sockfd, &watchFds))
            FD_SET(sockfd, &watchFds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        if(finish)
        {
            cout << "Time to close the connection"<<endl;
            //close(sockfd);
            break;
        }

        if ((nReadyFds = select(maxSockfd+1, &readFds, &writeFds, &errFds, &tv)) == -1) {
            perror("select");
            return 4;
        }
        
        if (nReadyFds == 0) {
            cout << "TIMEOUT HAPPENS" <<endl;
            if(state == SYNACK_SENT)
            {
                HeaderPacket resp_pkt_synack;
                resp_pkt_synack.m_seq = htons(mybuffer.getNextSeqNo());
                resp_pkt_synack.m_ack = htons(0);
                resp_pkt_synack.m_flags = htons(SYNACK); //SYNACK flag
                //resp_pkt_synack.m_window = htons(mybuffer.getWindowSize());
                if(sendto(sockfd, &resp_pkt_synack, sizeof(resp_pkt_synack), 0, (struct sockaddr*) &clientAddr, clilen)<0)
                {
                    cerr << "Sendto fails" << endl;
                    return 1;
                }
                 FD_SET(sockfd, &readFds);
            }
            else if (state == SENT_FIN)
            {
                HeaderPacket resp_pkt_fin;
                resp_pkt_fin.m_seq = htons(mybuffer.getNextSeqNo());
                resp_pkt_fin.m_flags = htons(FIN); //FIN flag
                resp_pkt_fin.m_ack = htons(0);
                //resp_pkt_fin.m_window = htons(mybuffer.getWindowSize());
                if(sendto(sockfd, &resp_pkt_fin, sizeof(resp_pkt_fin), 0, (struct sockaddr*) &clientAddr, clilen)<0)
                {
                    cerr << "Sendto fails" << endl;
                    return 1;
                }
                 FD_SET(sockfd, &readFds);
                
            }
            else if (state == CONNECTED)
            {
                if(mybuffer.timeout())
                {
                    FD_CLR(sockfd, &readFds);
                    //close after several retrials
                    close(sockfd);
                    break;
                }
                else
                {
                    uint16_t seq = mybuffer.retcumulative();
                    HeaderPacket res_pkt = mybuffer.getPkt(seq);
                    res_pkt.m_seq = htons(seq);
                    //res_pkt.m_window = htons(mybuffer.getWindowSize());
                    mybuffer.reset_pointer();
                    if(sendto(sockfd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr*) &clientAddr, clilen)<0)
                    {
                        cerr << "Sendto fails" << endl;
                        return 1;
                    }
                    FD_ZERO(&readFds);//clear the read set cuz we gonna retransmit again 
                    FD_SET(sockfd, &writeFds);
                }
            }
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
                        switch(ntohs(req_pkt.m_flags))
                        {
                            case SYNACK:
                            {
                                cout << "Receiveing SYNACK packet "<< ntohs(req_pkt.m_ack)<<endl;
                                mybuffer.setInitSeq(ntohs(req_pkt.m_ack));
                                if(myreader.hasMore())
                                {
                                    mybuffer.insert(myreader.top());
                                    myreader.pop();
                                }
                                else
                                {
                                    cout <<"nomore"<<endl;
                                }
                                state = CONNECTED;
                                FD_CLR(fd, &readFds);
                                FD_SET(fd, &writeFds);
                                break;
                            }
                            case ACK:
                            {
                                cout << "Receiving ACK packet "<< ntohs(req_pkt.m_ack)<<endl;
                                mybuffer.ack(ntohs(req_pkt.m_ack));
                                
                                /*
                                clock_t end = clock();
                                //reset the timeout
                                double sampleRTT = mybuffer.get_duration(ntohs(req_pkt.m_ack), end);
                                //now update the waiting time
                                
                                double difference = sampleRTT - sRTT;
                                sRTT = sRTT + 0.125 * (difference);
                                devRtt = devRtt + 0.25 * (abs(difference) - devRtt);
                                double ret_to = sRTT + 4* devRtt;
                                
                                double intpart, fracpart;
                                fracpart = modf(ret_to, &intpart);
                                tv.tv_sec = (time_t) intpart;
                                tv.tv_usec = (time_t) 1000000*fracpart;
                                */
                                while(myreader.hasMore() && mybuffer.hasSpace(strlen(myreader.top().payload)))
                                {
                                    cout << "let's do some insert" <<endl;
                                    mybuffer.insert(myreader.top());
                                    myreader.pop();
                                }
                                if(!mybuffer.hasNext())
                                {
                                    cout <<"change state to finish"<<endl;
                                    state = SENT_FIN;
                                }
                            
                                //don't do write until you ack all the packets you just sent out
                                if(mybuffer.switch_to_write())
                                {
                                    cout << "SWITCH TO WRITE STATE " <<endl;
                                    FD_CLR(fd, &readFds);
                                    FD_SET(fd, &writeFds);
                                }
                                break;
                            }
                            case FINACK:
                            {
                                cout << "Receiving FINACK packet "<< ntohs(req_pkt.m_ack)<<endl;
                                finish = true;
                                FD_CLR(fd, &readFds);
                                break;
                            }
                            default:
                                break;
                        }
                    }
                    
                }
                
                else if(FD_ISSET(fd, &writeFds)) // writing, sendto cases
                {
                        cout <<"Do some write" <<endl;

                        HeaderPacket res_pkt;
                        switch(state)
                        {
                            case CONNECTED:
                            {
                                uint16_t seq = mybuffer.getNextSeqNo();
                                cout <<"state is connected"<<endl;
                                res_pkt = mybuffer.getPkt(seq); // initial sequence number
                            
                               // clock_t t = clock();
                                
                                ///mybuffer.record_start(seq, clock());
                                
                                cout << "Send out data packet " << seq << endl;
                                
                                cout << "print out payload" <<endl;
                                int p_size = strlen(res_pkt.payload);
                                if(p_size >1024)
                                    p_size = 1024;
                                for (int i = 0; i<p_size; i++) {
                                    cout << res_pkt.payload[i];
                                }
                                cout << endl;
                                
                                if(sendto(sockfd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr*) &clientAddr, clilen)<0)
                                {
                                    cerr << "Sendto fails" << endl;
                                    return 1;
                                }
                                if(mybuffer.switch_to_read())
                                {
                                    cout << "SWITCH TO READ STATE" <<endl;
                                    FD_CLR(fd, &writeFds);
                                    FD_SET(fd, &readFds);
                                }
                                break;
                            }
                            case SENT_FIN:
                            {
                                uint16_t seq = mybuffer.retcumulative();
                                cout << "state is fin now" <<endl;
                                res_pkt.m_seq = htons(seq);
                                //res_pkt.m_window = htons(getWindowSize());
                                res_pkt.m_ack = htons(0);
                                res_pkt.m_flags = htons(FIN);
                                cout << "Sending FIN packet " << seq << endl;
                                if(sendto(sockfd, &res_pkt, sizeof(res_pkt), 0, (struct sockaddr*) &clientAddr, clilen)<0)
                                {
                                    cerr << "Sendto fails" << endl;
                                    return 1;
                                }
                                FD_CLR(fd, &writeFds);
                                FD_SET(fd, &readFds);
                                break;
                            }
                        }
                            
                    }
            }
        }
     }
    
    close(sockfd);
    }
