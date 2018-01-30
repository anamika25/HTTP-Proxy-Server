#include<iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <pthread.h>
#include <string>
#include <unistd.h>

using namespace std;


int main(int argc, char** argv)
{
	struct sockaddr_in server_addr;
	struct hostent *server_ip;
	int sockfd;

	if(argc != 4)
	{
		cout<<"enter proxy server ip, port and url"<<endl;
	}

	server_ip= gethostbyname(argv[1]);
	int port= atoi(argv[2]);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	memcpy(&server_addr.sin_addr.s_addr, server_ip->h_addr, server_ip->h_length);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Error in creating Client Socket!\n");
		exit(1);
	}

	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
	{
		close(sockfd);
		perror("Connection to Server Failed!\n");
		exit(1);
	}

	//construct get request
	string url= argv[3];
	string temp=url.substr(7);
	int pos=temp.find_first_of('/');
	string domain= temp.substr(0,pos);
	string filename="/" + temp.substr(pos+1);
	string message= "GET "+ filename +" HTTP/1.0\r\nHost: "+ domain +" \r\nUser-Agent: HTMLGET 1.0\r\n\r\n";

	char *buf_send=new char[message.length() + 1];
	strcpy(buf_send, message.c_str());

	char *file=new char[filename.length() + 1];
	strcpy(file,filename.c_str() );

	if(send(sockfd, (char *)buf_send, strlen(buf_send),0)<0)
	{
		perror("error in sending message\n");
	}

	//Download and save file received from proxy server
	FILE* fp;//= fopen(file,"w");
	char buf_recv[BUFSIZ+1];
	memset(buf_recv,0,sizeof(buf_recv));
	int nbytes=0;
	int exist = 0;
	//cout<<"tread lightly"<<endl;
	while((nbytes = recv(sockfd,buf_recv,BUFSIZ,0)) > 0)
	{
		//cout<<"say my name"<<endl;
		cout<<"buf_recv:"<<endl<<buf_recv<<endl;
		if(exist == 0)
		{
			//cout<<"exist==0"<<endl;
			exist++;
			size_t found = filename.find_last_of("/");
			filename =filename.substr(found+1);
			cout<<"file:"<<filename<<endl;
			strcpy(file,filename.c_str() );
			fp= fopen(file,"w+");
		}
		fputs(buf_recv,fp);
	}
	if(exist==0) cout<<"file does not exist";

	fclose(fp);
	close(sockfd);

	return 0;
}
