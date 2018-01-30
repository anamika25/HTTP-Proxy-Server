First use the makefile to compile the server.cpp and client.cpp then use the following steps to run the server and the clients. For running server we need IP and PORT number.

For Server:
./proxy_server <ip> <port>

example:

./proxy_server 127.0.0.1 8910

For Client:
./client <proxy ip> <port> <URL>

example
./client 127.0.0.1 8910 http://beej.us/guide/bgnet/output/html/Makefile

Ip address is the Loopback address/127.0.0.1


Code Architecture:

gethostbyname(): To get a structure of type hostent for the given host name. From there we get the address of the domain or host we are trying to send the request to.
socket(): To get the file descriptor.
bind(): To associate that socket with a port.

The client makes a simple TCP connection with the proxy server initially. Once the
connection is successful, the client sends the HTTP request to the proxy server. The proxy server parses the HTTP request to get the domain name and the page that is requested. It checks, if a corresponding entry is present in the LRU cache. Initially the LRU cache is empty thus the proxy makes a TCP connection with the domain server and sends an HTTP GET request on top of it. The domain server can either send the file or send a 404 type Error.  The proxy puts the page into LRU cache and forwards it to the client and in case of an error, it simply forwards it to the client. 

If there exists an entry which is not expired, proxy server sends the page to the client right away. If the entry is expired, it makes a conditional get request to the original domain. If it receives a 304 response, that means the entry has not been modified and thus, it can be sent to the client from the cache without any further delay.
If the entry has been modified at the domain server, it simply sends a 200 OK response and blocks of the page. The proxy updates the cache and once the page is received completely from the domain, it sends the page to the client.

In case of a new page being received from a domain server and the LRU cache being filled already, the “Least Recently Used” eviction policy is used to remove a page from the Cache and make space for the new entry. The last accessed field is used to make this decision, the page with the smallest value in this field is evicted. The last accessed field
is consistently updated whenever an entry is sent to a client from the cache.   
