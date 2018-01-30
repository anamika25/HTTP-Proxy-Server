/*
* server.cpp
*
*  Created on: Oct 20, 2016
*      Author: anamika, somnath
*/

#define RSP_SIZE 10240
#define HTTP_PORT "80"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <inttypes.h>
#include <stdarg.h>
#include <unistd.h>

using namespace std;

typedef struct
{
	int client_sock;
	int server_sock;
	string path;
	string host;
}client;

typedef struct
{
	char block[RSP_SIZE];
	int size;
}datum;

typedef struct
{
	vector<datum> data;;
	bool noExpiry;
	time_t last_accessed, expiry_time;
	string last_mod;
}cached_pages;

map<string, cached_pages> lru;
map<string, cached_pages> temp_data;

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void extract_info(char buf[RSP_SIZE], client& client_t)
{
	//cout<<"extract info"<<endl;
	string s(buf);
	size_t index1 = s.find("GET ");
	index1 += strlen("GET ");
	size_t index2 = s.find(" HTTP");
	string filename = s.substr(index1, index2 - index1);
	//cout<<"filename:"<<filename<<endl;
	index1 = s.find("Host: ");
	index1 += strlen("Host: ");
	index2 = s.find("\r\nUser-Agent:");
	string host = s.substr(index1, index2 - index1);
	client_t.host = host;
	client_t.path = filename;
	//cout<<"inside extract indoo"<<endl;
	//cout<<"host:"<<host<<endl;
	//cout<<"path:"<<filename<<endl;
}


int connect_to_host(string host)
{
	//cout<<"in connect_to_host"<<endl;
	//cout<<"host:"<<endl;
	host= "beej.us";
	//host= "man7.org";

	char *httphost=new char[host.length()+1];
	strcpy(httphost,host.c_str());
	struct hostent *newserver_ip;
	newserver_ip=gethostbyname(httphost);

	/*if(newserver_ip==NULL)
		cout<<"NULL"<<endl;
	else
		cout<<"name:"<<newserver_ip->h_name<<endl;*/

	int sockfd, rv;
	char host_name[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(newserver_ip->h_name, HTTP_PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("connect");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "failed to connect to host\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
		host_name, sizeof host_name);
	printf("connecting to host: %s\n", host_name);
	freeaddrinfo(servinfo); // all done with this structure
	return sockfd;
}

//if the header exists, returns true and saves it's value in output else returns false
bool extract_head(const char* header, char* buf, char* output)
{
	char *start = strstr(buf, header);
	if (!start) return false;

	char *end = strstr(start, "\r\n");
	start += strlen(header);
	while (*start == ' ') ++start;
	while (*(end - 1) == ' ') --end;
	strncpy(output, start, end - start);
	output[end - start] = '\0';
	return true;
}

int main(int argc, char* argv[])
{

	if (argc != 3)
	{
		cout << "please enter server, server_ip_address, server_port \n" << endl;
	}
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	map<int, client> clients;

	int fdmax; // maximum file descriptor number
	int listener; // listening socket descriptor
	int newfd; // newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	int nbytes;
	char remoteIP[INET6_ADDRSTRLEN];
	int yes = 1; // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;
	struct addrinfo hints, *ai, *p;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	//hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &ai)) != 0) {
		fprintf(stderr, "select of server: %s\n", gai_strerror(rv));
		exit(1);
	}

	for (p = ai; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{
			continue;
		}

		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			continue;
		}
		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL)
	{
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

					  // listen
	if (listen(listener, 100) == -1)
	{
		perror("listen");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	for (;;)
	{
		//cout<<"lru size: "<<lru.size()<<endl;
		read_fds = master; // copy it
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("select");
			exit(4);
		}

		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &read_fds))
			{
				// we got one!!
				if (i == listener)
				{
					//cout<<"i==listener"<<endl;
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
					if (newfd == -1)
					{
						perror("accept");
					}
					else
					{
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax)
						{
							// keep track of the max
							fdmax = newfd;
						}
						//client_list add
						client cli;
						cli.client_sock = newfd;
						cli.server_sock = -1;
						clients[newfd] = cli;

						printf("select server: new connection from %s on socket %d\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
							remoteIP, INET6_ADDRSTRLEN), newfd);
					}
				}
				else  //existing client or server
				{

					//cout<<"existing client"<<endl;
					char buf[RSP_SIZE];
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
					{
						if (nbytes < 0)
							perror("recv");
						//client_list erase
						//cout<<"closing conn bcoz nbytes<0"<<endl;
						close(i);
						FD_CLR(i, &master);
						clients.erase(i);
					}
					else
					{
						if (clients[i].client_sock == i) //existing client
						{
							cout<<"present in lru? ";
							//cout<<"in client sock==i"<<endl;
							extract_info(buf,clients[i]);
							string tmp = clients[i].host + clients[i].path;
							//string tmp = clients[i].path;
							//cout<<"temp: "<<tmp<<endl;
							if (lru.find(tmp) == lru.end())  //page not present in the lru
							{
								cout<<"no"<<endl;
								//cout<<"clients[i].host:"<<clients[i].host<<endl;
								//cout<<"clients[i].path:"<<clients[i].path<<endl;
								//request server for the page
								clients[i].server_sock = connect_to_host(clients[i].host);

								//cout<<"clients[i].server_sock"<<clients[i].server_sock<<endl;
								clients[clients[i].server_sock].client_sock = i;
								clients[clients[i].server_sock].server_sock = clients[i].server_sock;
								clients[clients[i].server_sock].host= clients[i].host;
								clients[clients[i].server_sock].path=clients[i].path;

								FD_SET(clients[i].server_sock, &master); // add to master_set set
								if (clients[i].server_sock > fdmax)
									fdmax = clients[i].server_sock;

				                //send GET request to the server for the page
								if (send(clients[i].server_sock, buf, nbytes, 0) == -1)
								{
									perror("send");
								}

							}
							else
							{
								cout<<"yes"<<endl;
								//expiry, modification
								if (lru[tmp].noExpiry)  //check noExpiry, if it is true we don't need to check any further, just send to client
								{
									cout<<"noExpiry is true so sending directly from cache"<<endl;
									for (int j  = 0; j < lru[tmp].data.size(); j++)
									{
										if (send(i, lru[tmp].data[j].block, lru[tmp].data[j].size, 0) == -1)
											perror("send");
									}
									close(i);
									FD_CLR(i, &master);
									auto it = clients.find(i);
									clients.erase(it);
								}
								else //check if current time > expiry time, request the server for updated page
								{
									cout<<"noExpiry is false"<<endl;
									time_t current_time;
									time(&current_time);
									struct tm * tptr;
									tptr = gmtime(&current_time);

									if (mktime(tptr) < lru[tmp].expiry_time) //not expired, send the file to client directly
									{
										cout<<"not exppired yet"<<endl;
										for (int j = 0; j < lru[tmp].data.size(); j++)
										{
											if (send(i, lru[tmp].data[j].block, lru[tmp].data[j].size, 0) == -1)
												perror("send");
										}
										close(i);
										FD_CLR(i, &master);
										auto it = clients.find(i);
										clients.erase(it);
									}
									else  //expired, send Conditional GET to server
									{
										cout << "Expired! so sending conditional get"<<endl;
										string last_modified=lru[tmp].last_mod;
										string message= "GET " + clients[i].path + " HTTP/1.0\r\nHost: " + clients[i].host + "\r\n"  + "If-Modified-Since: " + last_modified + "\r\n" + "\r\n";
										clients[i].server_sock = connect_to_host(clients[i].host);
										clients[clients[i].server_sock].client_sock = i;
										clients[clients[i].server_sock].server_sock = clients[i].server_sock;
										clients[clients[i].server_sock].host= clients[i].host;
										clients[clients[i].server_sock].path= clients[i].path;

										char *buf_send=new char[message.length() + 1];
										strcpy(buf_send, message.c_str());

										FD_SET(clients[i].server_sock, &master); // add to master_set set
										if (clients[i].server_sock > fdmax)
											fdmax = clients[i].server_sock;

								        //send GET request to the server for the page
										if (send(clients[i].server_sock, buf_send, strlen(buf_send), 0) == -1)
											perror("send");

									}
								}
								//update last accessed field
								time_t current_time;
								time(&current_time);
								struct tm * tptr;
								tptr = gmtime(&current_time);
								lru[tmp].last_accessed = mktime(tptr);
							}
						}
						else if (clients[i].server_sock == i) //existing host server
						{

							//receive data from server
							string tmp = clients[i].host + clients[i].path;
							if (strstr(buf, "304 Not Modified")) //not modified response for conditional GET
							{
								cout<<"server responded 304 so page was not modified"<<endl;
								for (int j  = 0; j < lru[tmp].data.size(); j++)
								{
									if (send(i, lru[tmp].data[j].block, lru[tmp].data[j].size, 0) == -1)
										perror("send");
								}
								time_t current;
								time(&current);
								struct tm * TimeTm;
								TimeTm = gmtime(&current);
								lru[tmp].last_accessed=mktime(TimeTm);

								close(i);
								FD_CLR(i, &master);
								close(clients[i].client_sock);
								FD_CLR(clients[i].client_sock, &master);

								auto it = clients.find(clients[i].client_sock);
								clients.erase(it);
								it=clients.find(i);
								clients.erase(it);

							}
							else if (strstr(buf, "200 OK") || (temp_data.find(tmp) != temp_data.end())) //response 200 OK, new or modified
							{
								cout<<"server responded 200 OK so new page/page is modified/new block has arrived"<<endl;
								if (lru.find(tmp) == lru.end() && (temp_data.find(tmp) == temp_data.end())) //new entry
								{
									cached_pages pg;
									//will the 200 OK or 304 message come in the header all for all the data transfers
									char output[255];

									if(temp_data.find(tmp) == temp_data.end())
									{
										if (extract_head("Expires:", buf, output) )
										{
											//expires header present
											time_t expire_time;
											struct tm TimeTm = { 0 };
											char *TimeTmPointer = strptime(output, "%A, %d %B %Y %H:%M:%S %Z", &TimeTm);
											if (!TimeTmPointer)
												expire_time = 0;
											else
												expire_time = mktime(&TimeTm);
											pg.expiry_time = expire_time;
											pg.noExpiry = false;
											//cout<<"no expiry is false"<<endl;
										}
										else
										{
											//expires header not present, set noExpiry to true
											pg.noExpiry = true;
											//cout<<"no expiry set to true"<<endl;
										}
										char output_t[255];
										if (extract_head("Last-Modified:", buf, output_t))
										{
											string last_mod(output_t);
											pg.last_mod = last_mod;
										}
									}

									datum d;
									strcpy(d.block, buf);
									d.size = nbytes;
									vector<datum> rec;
									rec.push_back(d);
									pg.data = rec;

									temp_data[tmp] = pg;

								}
								if (lru.find(tmp) == lru.end() && (temp_data.find(tmp) != temp_data.end())) //continuing block
								{
									datum d;
									strcpy(d.block, buf);
									d.size = nbytes;
									temp_data[tmp].data.push_back(d);
								}
								if (lru.find(tmp) != lru.end() && (temp_data.find(tmp) == temp_data.end())) //modified
								{
									//remove entry from lru and save blocks in temp_data, copy whole entry in lru when complete
									lru.erase(tmp);

									cached_pages pg;
									char output[255];
									if(temp_data.find(tmp) == temp_data.end())
									{
										if (extract_head("Expires:", buf, output))
										{
											//expires header present
											time_t expire_time;
											struct tm TimeTm = { 0 };
											char *TimeTmPointer = strptime(output, "%A, %d %B %Y %H:%M:%S %Z", &TimeTm);
											if (!TimeTmPointer)
												expire_time = 0;
											else
												expire_time = mktime(&TimeTm);
											pg.expiry_time = expire_time;
											pg.noExpiry = false;
											//cout<<"no expiry is false"<<endl;
										}
										else
										{
											//expires header not present, set noExpiry to true
											pg.noExpiry = true;
											//cout<<"no expiry set to true"<<endl;
										}
										char output_t[255];
										if (extract_head("Last-Modified:", buf, output_t))
										{
											//Last-Modified header present
											/*time_t last_mod;
											struct tm TimeTm = { 0 };
											char *TimeTmPointer = strptime(output_t, "%A, %d %B %Y %H:%M:%S %Z", &TimeTm);
											if (!TimeTmPointer)
												last_mod = 0;
											else
												last_mod = mktime(&TimeTm);*/
											string last_mod(output_t);
											pg.last_mod = last_mod;
										}
									}

									datum d;
									strcpy(d.block, buf);
									d.size = nbytes;
									vector<datum> rec;
									rec.push_back(d);
									pg.data = rec;

									temp_data[tmp] = pg;
								}

								if (nbytes < RSP_SIZE) //last block received, copy data to cache, send data to client
								{

									if(lru.size() < 3)
									{
										cached_pages new_entry;
										new_entry.data = temp_data[tmp].data;
										new_entry.noExpiry = temp_data[tmp].noExpiry;
										new_entry.expiry_time = temp_data[tmp].expiry_time;
										new_entry.last_mod = temp_data[tmp].last_mod;

										//get current time
										time_t current;
										time(&current);
										struct tm * TimeTm;
										TimeTm = gmtime(&current);

										//update last_access time
										new_entry.last_accessed = mktime(TimeTm);

										//copy data to cache
										lru[tmp] = new_entry;
										//cout<<"check no expiry: "<<lru[tmp].noExpiry<<endl;
										cout<<"lru size:"<<lru.size()<<endl;

										temp_data.erase(tmp);

										//send data to client
										for(auto it=lru[tmp].data.begin();it!=lru[tmp].data.end();it++)
										{
											if(send(clients[i].client_sock, (it)->block, (it)->size, 0)==-1)
												perror("send");
										}

										close(clients[i].client_sock);
										FD_CLR(clients[i].client_sock, &master);
										close(i);
										FD_CLR(i, &master);

										clients.erase(clients[i].client_sock);
										clients.erase(i);

									}
									else
									{
										//evict least recently used and add this one.

										time_t current;
										time(&current);
										struct tm * TimeTm;
										TimeTm = gmtime(&current);
										time_t last_used=mktime(TimeTm);

										string str;
										for (auto it = lru.begin(); it != lru.end(); it++)
										{
											if (last_used > (it->second).last_accessed)
											{
												last_used = (it->second).last_accessed;
												str = it->first;
											}
										}
										cout<<"lru limit exceeded so evicting least recently used page : "<<str<<endl;
										lru.erase(str);

										cached_pages new_entry;
										new_entry.data = temp_data[tmp].data;
										new_entry.noExpiry = temp_data[tmp].noExpiry;
										new_entry.expiry_time = temp_data[tmp].expiry_time;
										new_entry.last_mod = temp_data[tmp].last_mod;

										//get current time
										time(&current);
										TimeTm = gmtime(&current);
										new_entry.last_accessed = mktime(TimeTm);
										lru[tmp] = new_entry;
										//cout<<"lru count:"<<lru.size()<<endl;

										for(auto it=lru[tmp].data.begin();it!=lru[tmp].data.end();it++)
										{
											if(send(clients[i].client_sock, (it)->block, (it)->size, 0)==-1)
												perror("send");
									    }
										close(clients[i].client_sock);
										FD_CLR(clients[i].client_sock, &master);
										close(i);
										FD_CLR(i, &master);

										clients.erase(clients[i].client_sock);
										clients.erase(i);
									}
								}
							}
							else
							{
								cout<<"server responded but it is neither 304 nor 200"<<endl;
								if (send(clients[i].client_sock, buf, nbytes, 0) == -1)
									perror("send");

								close(i);
								FD_CLR(i, &master);
								close(clients[i].client_sock);
								FD_CLR(clients[i].client_sock, &master);

								auto it = clients.find(clients[i].client_sock);
								clients.erase(it);
								it=clients.find(i);
								clients.erase(it);
							}

						}
					}
				}
			}
		}

	}

}
