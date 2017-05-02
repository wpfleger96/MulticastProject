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

using namespace std;

//Represents participants
struct Participant{
	int id;
	int port;
	//int socketFd;
	string ip;
	bool online;

	Participant(int id, int port, string ip, bool online);
	int getID();
	int getPort();
	//int getSocketFd();
	string getIP();
	bool isOnline();

};

struct ParticipantQueue{
	vector<Participant*> participants;

	void addParticipant(Participant*);
	void removeParticipant(Participant*);
	void removeByID(int);
	void goOffline(int);
	void goOnline(int);
	void printParticipants();
};

void log(string message);
void fatal_error(string output);
void executeThreadLoop(int, ParticipantQueue*);
int socketSetup(const char*);
int messageSocketSetup(const char*, const char*);

void ParticipantQueue::addParticipant(Participant* p){
	bool found = false;
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->getID() == p->getID()){
			participants.at(i)->id = p->id;
			participants.at(i)->ip = p->ip;
			participants.at(i)->online = p->online;
			found=true;
		}
	}
	if(!found){
		participants.push_back(p);
	}
}

void ParticipantQueue::removeParticipant(Participant* p){
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->getID() == p->getID()){
			int j=i;
			participants.erase(participants.begin() + i);
			return;
		}
	}
}

void ParticipantQueue::removeByID(int id){
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->getID() == id){
			participants.erase(participants.begin() + i);
			return;
		}
	}
}

void ParticipantQueue::goOffline(int id){
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->getID() == id){
			participants.at(i)->online = false;
			return;
		}
	}
}


void ParticipantQueue::goOnline(int id){
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->getID() == id){
			participants.at(i)->online = true;
			return;
		}
	}
}

void ParticipantQueue::printParticipants(){
	int active = 0;
	for(int i=0; i<participants.size(); i++){
		if(participants.at(i)->isOnline()) active++;
	}
	if(active ==0) cout << "No participants online" << endl;
	else{
		cout << "There are " << active << " participants online" << endl;
		for(int i=0; i<participants.size(); i++){
			if(participants.at(i)->isOnline()){
				cout << "Participant ID: " << participants.at(i)->getID() << " IP: " << participants.at(i)->getIP() << " and port: " << participants.at(i)->getPort();
				cout << endl;
			}
		}
	}
}

Participant::Participant(int id, int port, string ip, bool online){
	this->id = id;
	this->port = port;
	this->online=online;
	this->ip=ip;
	//this->socketFd = sockFd;
}

int Participant::getID(){
	return this->id;
}

/*
int Participant::getSocketFd(){
	return this->socketFd;
}
*/

int Participant::getPort(){
	return this->port;
}

string Participant::getIP(){
	return this->ip;
}

bool Participant::isOnline(){
	return this->online;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, char* argv[]){
	int socketFd, newSocketFd;
	int port, pTime;

	if(argc != 2){
		fatal_error("Usage: coordinator configFile");
	}

	fstream configFile(argv[1], std::fstream::in);

	configFile >> port >> pTime;
	cout << "Read port " << port << " and pTime " << pTime << " from config file" << endl;

	const char* portBuff;
	string s = std::to_string(port);
	portBuff = s.c_str();
	socketFd = socketSetup(portBuff);

	ParticipantQueue* pQueue = new ParticipantQueue();

	while(true){
		struct sockaddr_storage clientAddress;
		socklen_t socketLength = sizeof(clientAddress);

		if((newSocketFd = accept(socketFd,(struct sockaddr*)&clientAddress, &socketLength))==-1){
			//log("accept error");
		}

		thread connectionThread(executeThreadLoop, newSocketFd, pQueue);
		connectionThread.detach();
		//sleep(10);
	}

	return 0;
}

void executeThreadLoop(int newSocketFd, ParticipantQueue* pQueue){
	while(true){
		int readLength;
		char buffer[256];
		memset(buffer, 0, sizeof(buffer));

		log("participant thread waiting for request");
		if((readLength = recv(newSocketFd,buffer,256,0))==-1){//receive command from socket into buffer
			//log("CHILD read error");
		}

		std::string input(buffer);

		cout << "Coordinator processing " << input << endl;

		if(!input.compare(0,8,"register")){
			if(send(newSocketFd, "ACK", 3, 0) < 0){//ack register command
				log("error ACKing register");
			}

			int pos = input.find(" ");
			input = input.substr(pos+1);

			//Parse participant's ID from message received from socket
			pos = input.find(" ");
			string sub = input.substr(0,pos);
			int id = stoi(sub);

			//Parse user's IP address
			input = input.substr(pos+1);
			pos = input.find(" ");
			string ip = input.substr(0, pos);

			//Parse user's port number
			input = input.substr(pos+1);
			int port = stoi(input);

			//Add participant to the queue. If participant is already in the queue the function will just update the participant's information
			Participant* p = new Participant(id, port, ip, true);
			pQueue->addParticipant(p);

			log("Participant successfully registered");

		}
		else if(!input.compare(0,10,"deregister")){
			if(send(newSocketFd, "ACK", 3, 0) < 0){//ack deregister command
				log("error ACKing deregister");
			}
			int pos = input.find(" ");
			input = input.substr(pos+1);

			int id = stoi(input);
			
			pQueue->goOffline(id);

			log("Participant successfully deregistered");

		}
		else if(!input.compare(0,10,"disconnect")){
			if(send(newSocketFd, "ACK", 3, 0) < 0){//ack disconnect command
				log("error ACKing disconnect");
			}
			int pos = input.find(" ");
			input = input.substr(pos+1);

			int id = stoi(input);
			
			pQueue->goOffline(id);

			log("Participant successfully disconnected");

		}
		else if(!input.compare(0,10,"reconnect")){
			if(send(newSocketFd, "ACK", 3, 0) < 0){//ack reconnect command
				log("error ACKing reconnect");
			}

			int pos = input.find(" ");
			input = input.substr(pos+1);

			pos = input.find(" ");
			string sub = input.substr(0,pos);
			int id = stoi(sub);

			input = input.substr(pos+1);
			pos = input.find(" ");
			string ip = input.substr(0, pos);

			input = input.substr(pos+1);
			int port = stoi(input);

			Participant* p = new Participant(id, port, ip, true);

			pQueue->addParticipant(p);

			log("Participant succesfully reconnected");


		}
		//DOES NOT WORK YET
		else if(!input.compare(0,4,"test")){
			const char* buff;
			string s = "test message";
			buff = s.c_str();
			for(int i=0; i<pQueue->participants.size(); i++){
				//if(participants.at(i)->getID() != ID){
					string s = pQueue->participants.at(i)->getIP();
					const char* ip ;
					ip = s.c_str();
					s = pQueue->participants.at(i)->getPort();
					const char* port;
					port = s.c_str();
					int messageSocket = messageSocketSetup(ip, port);
					if(send(messageSocket, buff, sizeof(buff), 0) < 0){
						log("Error sending test message");
					}
					close(messageSocket);
				//}
			}
		}
		//DOES NOT WORK YET
		else if(!input.compare(0,5,"msend")){
			if(send(newSocketFd, "ACK", 3, 0) < 0){//ack msend command
				log("error ACKing msend");
			}

			input = input.substr(6);
			int pos = input.find(" ");
			int ID = stoi(input.substr(0,pos));
			input = input.substr(pos+1);

			cout << "Coordinator received message: " << input << " from participant "  << ID << endl;

			string message = input;
			const char* buff;
			buff = message.c_str();

			for(int i=0; i<pQueue->participants.size(); i++){
				if(pQueue->participants.at(i)->getID() != ID){
					string s = pQueue->participants.at(i)->getIP();
					const char* ip ;
					ip = s.c_str();
					s = pQueue->participants.at(i)->getPort();
					const char* port;
					port = s.c_str();
					int messageSocket = messageSocketSetup(ip, port);
					if(send(messageSocket, buff, sizeof(buff), 0) < 0){
						log("Error sending msend message");
					}
					close(messageSocket);
				}
			}

		}


	}

}



void log(string message){
	cout << message << endl;
}

void fatal_error(string output){
	cout << output << endl;
	exit(EXIT_FAILURE);	
}

//Main socket setup
int socketSetup(const char *portNum){

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
		fatal_error("Error on addr info port");
	}

	log("addr info success");

	for(struct addrinfo *addrOption = serverInfo; addrOption!=NULL; addrOption = addrOption->ai_next){

		if((socketFd = socket(addrOption->ai_family, addrOption->ai_socktype, addrOption->ai_protocol)) == -1){
			fatal_error("socket creation failed");
			continue;
		}
		log("socket created");
		if(bind(socketFd, addrOption->ai_addr, addrOption->ai_addrlen) == -1){
			close(socketFd);
			fatal_error("bind failed");
			continue;
		}
		log("socket bound");
		freeaddrinfo(serverInfo);

		break;
	}

	if(socketFd == -1){
		fatal_error("socket creation failed");
	}

	if(listen(socketFd, 5) == -1){
		fatal_error("error on listen");
	}
	log("socket listening");

	return socketFd;
}


//Set up socket to send messages to participant
int messageSocketSetup(const char* ip, const char* port){

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
	int newSocketFd=-1;
	for(struct addrinfo *addrOption = serverInfo; addrOption!=NULL; addrOption = addrOption->ai_next){

		if((newSocketFd = socket(addrOption->ai_family, addrOption->ai_socktype, addrOption->ai_protocol)) == -1){
			//error("socket creation failed");
			continue;
		}
		//log("socket created");

		if (connect(newSocketFd, addrOption->ai_addr, addrOption->ai_addrlen) == -1) {
			close(newSocketFd);
			//error("error on connect");
			continue;
		}
		log("connected to server");
		freeaddrinfo(serverInfo);

		break;
	}

	if(newSocketFd == -1){
		fatal_error("socket creation failed");
	}

	log("Socket creation completed with fd "+to_string(newSocketFd));

	return newSocketFd;

}

