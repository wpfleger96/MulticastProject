#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <time.h>
#include <thread>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

void log(string);
void fatal_error(string);
void error(string);
int clientSocketSetup(char* , const char*);
void handleCommand(string);
void parseCommand();
void messageLoop(int, string);
int messageSocketSetup(const char*);

string logFile;
int port, listenPort, id;
int socketFd, messageSocketFd; 
char* ip;
bool online=false;

int main(int argc, char* argv[]){
	fstream configFile(argv[1], std::fstream::in);
	configFile >> id >> logFile;
	char* buff = new char[25];
	configFile >> buff;	
	ip = strtok(buff, ":");
	port = stoi(strtok(NULL, "\n"));

	//Get message listening port from command line for debugging purposes
	listenPort = stoi(argv[2]);
	
	cout << "Received id " << id << ", logFile " << logFile << ", ip " << ip << ", and port " << port << " from config file" << endl;

	//Socket for sending messages to coordinator
	const char* portBuff;
	string s = std::to_string(port);
	portBuff = s.c_str();
	socketFd = clientSocketSetup(ip, portBuff);

	//Socket for receiving messages from coordinator
	s = std::to_string(listenPort);
	memset(buff, 0, sizeof(buff));
	portBuff = s.c_str();
	messageSocketFd = messageSocketSetup(portBuff);


	thread messageThread(messageLoop, messageSocketFd, logFile);
	messageThread.detach();
	
	string input;
	while(true){
		cout << "participant>";	
		getline(cin, input);
		handleCommand(input);
	}

	return 0;
}


void handleCommand(string command){
	cout << "Participant handling " << command << endl;
	int commandLength = command.length();
	if(!command.compare(0,8,"register") && commandLength == 8){
		
		string message;
		message += "register";
		message += " ";
		message += to_string(id);
		message += " ";
		message += ip;
		message += " ";
		message += to_string(listenPort);

		cout << "Registering user " << id << " with port " << listenPort << endl;

		const char* buff;
		buff = message.c_str();

		//Check if message socket is still open
		int r;
		r = fcntl(messageSocketFd, F_GETFL);
		if(r == -1){
			cout << "Reopening message socket" << endl;
			messageSocketFd = messageSocketSetup(buff);
		}

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending register message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack

		online=true;

	}
	if(!command.compare(0,10,"deregister") && commandLength == 10){
		online=false;

		string message = "deregister " + to_string(id);

		const char* buff;
		buff = message.c_str();

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending deregister message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack

		close(messageSocketFd);
		sleep(1);
	}
	else if(!command.compare(0,10,"disconnect") && commandLength == 10){
		online=false;

		string message = "disconnect " + to_string(id);

		const char* buff;
		buff = message.c_str();

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending disconnect message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack

		close(messageSocketFd);
		sleep(1);
	}
	else if(!command.compare(0,8,"reconnect") && commandLength == 8){
		const char* portBuff;
		listenPort = stoi(command.substr(10));
		string s = std::to_string(listenPort);
		portBuff = s.c_str();
		messageSocketFd = messageSocketSetup(portBuff);

		string message;
		message += "reconnect";
		message += " ";
		message += to_string(id);
		message += " ";
		message += ip;
		message += " ";
		message += to_string(listenPort);

		const char* buff;
		buff = message.c_str();

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending reconnect message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack

		online=true;
	}
	else if(!command.compare(0,5,"msend") && commandLength == 5){
		cout << "Participant handling msend command" << endl;
		string message = "msend " + to_string(id) + " " + command.substr(6);
		const char* buff;
		buff = message.c_str();

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending mcast msend message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack
	}
	//Block to test msend functionality
	else if(!command.compare(0,4,"test") && commandLength == 4){
		const char* buff;
		buff = command.c_str();

		if(send(socketFd,buff,strlen(buff),0) == -1){
			error("Error sending test message");
		}

		char ackBuf[3];
		recv(socketFd,ackBuf,3,0);//command ack
	}

	
}

void messageLoop(int socketFd, string logfile){
	fstream fstr;
	fstr.open(logfile.c_str(), std::ios::app);
	log("Message thread waiting for message from coordinator");
	int readLen;
	char* buffer[256];
	while(true){
		if(online){
			memset(buffer, 0, 256);
			sleep(1);
			if((readLen = recv(messageSocketFd,buffer,256,0))==-1){//receive command from socket into buffer
			//error("Error receving mesage from coordinator");
			}
			else{
				buffer[readLen] = '\0';
				//string message(buffer);
				cout << "Received following message from coodinator: " << buffer << endl;
				fstr << buffer;
			}
		}
	
	}
}

int clientSocketSetup(char* ip, const char* port){

	struct addrinfo prepInfo;
	struct addrinfo *serverInfo;

	memset(&prepInfo, 0, sizeof prepInfo);
	prepInfo.ai_family = AF_UNSPEC;
	prepInfo.ai_socktype = SOCK_STREAM;

	int code;
	if((code = getaddrinfo(ip,port,&prepInfo,&serverInfo))!=0){
		cout<<gai_strerror(code)<<'\n';
		fatal_error("Error on addr info");
	}
	int socketFd=-1;
	for(struct addrinfo *addrOption = serverInfo; addrOption!=NULL; addrOption = addrOption->ai_next){

		if((socketFd = socket(addrOption->ai_family, addrOption->ai_socktype, addrOption->ai_protocol)) == -1){
			//error("socket creation failed");
			continue;
		}
		//log("socket created");

		if (connect(socketFd, addrOption->ai_addr, addrOption->ai_addrlen) == -1) {
			close(socketFd);
			//error("error on connect");
			continue;
		}
		log("connected to server");
		freeaddrinfo(serverInfo);

		break;
	}

	if(socketFd == -1){
		fatal_error("socket creation failed");
	}

	log("Socket creation completed with fd "+to_string(socketFd));

	return socketFd;

}

int messageSocketSetup(const char *portNum){

	int socketFd = -1;
	struct addrinfo prepInfo;
	struct addrinfo *serverInfo;

	memset(&prepInfo, 0, sizeof(prepInfo));
	prepInfo.ai_family = AF_UNSPEC;
	prepInfo.ai_socktype = SOCK_STREAM;
	prepInfo.ai_flags = AI_PASSIVE; // sets local host

	int code;
	if((code=getaddrinfo(NULL,portNum,&prepInfo,&serverInfo))!=0){//uses prep info to fill server info
		cout<<gai_strerror(code)<<'\n';
		error("Error on addr info port");
	}

	log("addr info success");

	for(struct addrinfo *addrOption = serverInfo; addrOption!=NULL; addrOption = addrOption->ai_next){

		if((socketFd = socket(addrOption->ai_family, addrOption->ai_socktype, addrOption->ai_protocol)) == -1){
			error("socket creation failed");
			continue;
		}
		log("socket created");
		if(bind(socketFd, addrOption->ai_addr, addrOption->ai_addrlen) == -1){
			close(socketFd);
			error("bind failed");
			continue;
		}
		log("socket bound");
		freeaddrinfo(serverInfo);

		break;
	}

	if(socketFd == -1){
		error("socket creation failed");
	}

	if(listen(socketFd, 5) == -1){
		error("error on listen");
	}
	log("socket listening");

	return socketFd;
}



void log(string message){
	cout << message << endl;
}

void fatal_error(string output){
	cout << output << endl;
	exit(EXIT_FAILURE);	
}

void error(string message){
	cout << message << endl;
}
