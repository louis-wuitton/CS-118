//we are implementing request and response messages. We use inheritnace
//note that for request, when writing the request line
//we need to specify method, URL, and HTTP
//Therefore we need at least 3 functions
//setmethod, setURL and setHTTP
//for setmethod most likely we will deal with GET. 
//It's necessary to implement another function called setConnection
//when its close, that means we want to do non-persistent
//otherwise, we will do pipelining

//in either case, the message should have emtpy lines

//how do we actually put the real data into part of our response message

#ifndef HTTP_H
#define HTTP_H

struct HttpVersion
{
    std::string m_versionstr;
};


struct HttpMethod
{
    std::string m_methodstr;
};

struct HttpStatus
{
    std::string m_statuscode;
};

 
//////////////////// HttpMessage Class //////////////////////

class HttpMessage 
{
 public:
 //set connection method. This should be shared
    virtual void decodeFirstLine(std::vector<char> line) = 0;
    virtual std::vector<char> encode() = 0;
    HttpVersion getVersion()
    {
        return m_version;
    }
    void setVersion (HttpVersion version);
    void setHeader(std::string key, std::string value);
    std::string getMap(std::string);
    void decodeHeaderline(std::vector<char> line);
    void setPayLoad(std::vector<char> blob);
    std::vector<char> getPayload();
    
 private:
    HttpVersion m_version;
    std::vector<char> m_payload;
    std::map<std::string, std::string> m_headers;
};

/////////////////// HttpRequest Class /////////////////////

class HttpRequest
	:public HttpMessage
{
 public:
    virtual void decodeFirstLine(std::vector<char> line);
    HttpMethod getMethod();
    void setMethod(HttpMethod method);
    std::string getUrl();
    void setUrl(std::string url);
    //encode the request into the wired form
    virtual std::vector<char> encode();
 private:
    std::string m_url;
    HttpMethod m_method;
};

///////////////////////////// HttpResponse Class ////////////////

class HttpResponse
	:public HttpMessage
{
 public:
    virtual void decodeFirstLine(std::vector<char> line);
    HttpStatus getStatus();
    void setStatus(HttpStatus status);
    std::string getDescription();
    void setDescription(std::string description);
    virtual std::vector<char> encode();
 private:
    HttpStatus m_status;
    std::string m_statusDescription;
    bool is_full_URL;
};

#endif
