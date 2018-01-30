client.o: proxy_server.o
	g++ -std=c++0x -w   -o  client  client.cpp
proxy_server.o: 
	g++ -std=c++0x -w   -o  proxy_server  proxy_server.cpp
clean:
	rm -rf proxy_server
	rm -rf client