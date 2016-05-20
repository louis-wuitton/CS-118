#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <vector>
#include "httpmsg.h"

//////////////////////////////// Implement functions for HttpMessage ///////////////

void HttpMessage::setHeader(std::string key, std::string value)
{
    m_headers[key] = value;
}

std::string HttpMessage::getMap(std::string key)
{
    if(m_headers.find(key) == m_headers.end())
        return "Cannot find this key";
    else
        return m_headers[key];
}

void HttpMessage::decodeHeaderline(std::vector<char> line)
{
    //skip the first line
    int i = 0;
    int j = 0;
    while (line[i] != '\r')
        i++;
    if(line[i] == '\r' && line[i+1] == '\n' && line[i+2] == '\r' && line[i+3] == '\n')
        return;
    i+=2;
    //decoding the lines
    char key[100] = {'\0'};
    char value[100] = {'\0'};
    while (1) {
        memset(key, '\0', sizeof(key));
        memset(value, '\0', sizeof(value));
        //getting the key
        while(1)
        {
            if(line[i]==':')
                break;
            key[j] = line[i];
            i++;
            j++;
        }
        j = 0;
        i+=2;
        
        while(1)
        {
            if(line[i]=='\r')
                break;
            value[j] = line[i];
            i++;
            j++;
        }
        
        //get the key and value, not set them
        std::string m_key = key;
        std::string m_value = value;
        setHeader(m_key, m_value);
        if(line[i] == '\r' && line[i+1] == '\n' && line[i+2] == '\r' && line[i+3] == '\n')
            break;
        else if (line[i] == '\r' && line[i+1] == '\n' && line[i+2] != '\r')
        {
            i+=2;
            j=0;
            continue;
        }
    }
}

void HttpMessage::setVersion(HttpVersion version)
{
    m_version = version;
}

void HttpMessage::setPayLoad(std::vector<char> blob)
{
    m_payload = blob;
}

std::vector<char> HttpMessage::getPayload()
{
    return m_payload;
}


///////////////////// Implement functions for HttpRequest //////////////////////////
void HttpRequest::decodeFirstLine(std::vector<char> line)
{
    //decode the incoming line and set request
    char http_method[5] = {'\0'};
    char fileurl[100] = {'\0'};
    char http_version[9] = {'\0'};
    int i = 0;
    int j = 0;
    
    while (line[i]!= ' ')
    {
        http_method[i] = line[i];
        i++;
    }
    
    m_method.m_methodstr = http_method;
    while(line[i]==' ')
        i++;
    while (line[i] != ' ')
    {
        fileurl[j] = line[i];
        j++;
        i++;
    }
    
    m_url = fileurl;
    while(line[i]==' ')
        i++;
    j = 0;
    while (line[i] != '\r')
    {
        http_version[j] = line[i];
        i++;
        j++;
    }
    HttpVersion version;
    version.m_versionstr = http_version;
    setVersion(version);
}





HttpMethod HttpRequest::getMethod()
{
    return m_method;
}

void HttpRequest::setMethod(HttpMethod method)
{
    m_method = method;
}

std::string HttpRequest::getUrl()
{
    return m_url;
}

void HttpRequest::setUrl(std::string url)
{
    m_url = url;
}


std::vector<char> HttpRequest::encode()
{
    std::vector<char> wire;
    int i;
   
    for(i = 0; i < m_method.m_methodstr.length(); i++)
    {
        wire.push_back(m_method.m_methodstr[i]);
    }
    wire.push_back(' ');
    
    //parsing the m_url
    i = 0;
    int j = 0;
    char hostname[100] = {'\0'};
    std::cout << "my url is " << m_url <<std::endl;
    if(m_url[i] == 'h' && m_url[i+1] == 't'&& m_url[i+2] == 't'&&m_url[i+3] == 'p')
    {
        std::cout << "got it"<<std::endl;
        i+=7;
        while(1)
        {
            if(m_url[i]=='/')
                break;
            hostname[j]= m_url[i];
            i++;
            j++;
        }
    }
    std::string my_host = hostname;
    setHeader("Host", my_host);
   
    std::cout <<"hostname is " << my_host <<std::endl;
    while(i < m_url.size())
    {
        wire.push_back(m_url[i]);
        i++;
    }
    wire.push_back(' ');
    
    
    for(i=0; i< getVersion().m_versionstr.size(); i++)
    {
        wire.push_back(getVersion().m_versionstr[i]);
    }
    wire.push_back('\r');
    wire.push_back('\n');
    
    
    std::string conn = "Connection: ";
    for (int i = 0; i < conn.length(); i++)
        wire.push_back(conn[i]);
    
    std::string con_key =getMap("Connection");
    for (int i = 0; i < con_key.length(); i++)
        wire.push_back(con_key[i]);
    
    wire.push_back('\r');
    wire.push_back('\n');

    std::string hou = "Host: ";
    for (int i = 0; i < hou.length(); i++)
        wire.push_back(hou[i]);
    
    std::string hou_key =getMap("Host");
    for (int i = 0; i < hou_key.length(); i++)
        wire.push_back(hou_key[i]);
    
    wire.push_back('\r');
    wire.push_back('\n');
    
    wire.push_back('\r');
    wire.push_back('\n');
    return wire;
}

///////////////////////// Implement functions for HttpResponse //////////////////////
void HttpResponse::setStatus(HttpStatus status)
{
    m_status = status;
}

void HttpResponse::setDescription(std::string description)
{
    m_statusDescription = description;
}



void HttpResponse::decodeFirstLine(std::vector<char> line)
{
    char http_version[9];
    char http_status[4];
    char description[20];
    int i = 0;
    int j = 0;
    while (line[i] != ' ')
    {
        http_version[i] = line[i];
        i++;
    }
    while(line[i]==' ')
        i++;
    HttpVersion version;
    version.m_versionstr = http_version;
    setVersion(version);

    //set the status
    while (line[i] != ' ')
    {
        http_status[j] = line[i];
        i++;
        j++;
    }
    m_status.m_statuscode = http_status;
    
    while(line[i]==' ')
        i++;
    j=0;
    
    while (line[i] != '\r') {
        description[j] = line[i];
        i++;
        j++;
    }
    m_statusDescription = description;
}



HttpStatus HttpResponse::getStatus()
{
    return m_status;
}


std::string HttpResponse::getDescription()
{
    return m_statusDescription;
}



std::vector<char> HttpResponse::encode()
{
    std::vector<char> wire;
    int i;

    //push the HTTP version
    for(i = 0; i< getVersion().m_versionstr.length(); i++)
    {
        wire.push_back(getVersion().m_versionstr[i]);
    }
    wire.push_back(' ');
    
    for(i = 0; i < m_status.m_statuscode.length(); i++)
    {
        wire.push_back(m_status.m_statuscode[i]);
    }
    wire.push_back(' ');
    
    //push the URL
    for (i=0; i < m_statusDescription.length(); i++)
    {
        wire.push_back(m_statusDescription[i]);
    }
    wire.push_back(' ');
    wire.push_back('\r');
    wire.push_back('\n');
    
    
    std::string conn = "Connection: ";
    for (int i = 0; i < conn.length(); i++)
       wire.push_back(conn[i]);
    
    std::string con_key =getMap("Connection");
    for (int i = 0; i < con_key.length(); i++)
        wire.push_back(con_key[i]);
    
    wire.push_back('\r');
    wire.push_back('\n');
    
    std::string con_len = "Content-Length: ";
    for (int i = 0; i < con_len.length(); i++)
        wire.push_back(con_len[i]);
    
    std::string len_key = getMap("Content-Length");
    for (int i = 0; i < len_key.length(); i++)
        wire.push_back(len_key[i]);
    
    //end of the message, add another line to separate the payload
    wire.push_back('\r');
    wire.push_back('\n');
    wire.push_back('\r');
    wire.push_back('\n');
    return wire;

}




