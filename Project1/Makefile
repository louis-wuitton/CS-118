CXX=g++
CXXFLAGS= -g -pthread -std=c++11 

all: web-server web-client web-server-async web-client1

web-server: $(CLASSES)
	$(CXX) $(CXXFLAGS) web-server.cpp httpmsg.cpp -o $@

web-client: $(CLASSES)
	$(CXX) $(CXXFLAGS) web-client.cpp httpmsg.cpp -o $@

web-server-async: $(CLASSES)
	$(CXX) $(CXXFLAGS) web-server-async.cpp httpmsg.cpp -o $@

web-client1: $(CLASSES)
	$(CXX) $(CXXFLAGS) web-client.cpp httpmsg1.cpp -o $@

project1_files = README.md  Makefile httpmsg.h httpmsg.cpp httpmsg1.cpp web-server.cpp web-client.cpp web-server-async.cpp
project1-TianyanWuRyanLi.tar.gz : $(project1_files)
	tar -czf $@ $(project1_files)

submission: project1-TianyanWuRyanLi.tar.gz

clean:
	rm -rf *.o *~ *.gch *.swp *.tar.gz web-server web-server-async web-client web-client1



