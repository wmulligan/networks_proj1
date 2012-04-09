/* client_app_layer.cpp
 * Will Mulligan
 * Klevis Luli
 */
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include "client_app_layer.h"
#include "queue.h"
#include "client_func.h"

using namespace std;

void * ApplicationLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  int loggedIn = 0; //user authenticated?
  

  cout << "[Application] Initializing..." << endl;
 
  // Print welcome msg
  app_welcomeMsg();

  while(1){
	  int loginattempt = 0;

          char *pInput; //input string 
	  pInput = (char *) malloc(sizeof(char) * 256);
	  cout<<"[Application] >> ";
	  memset(pInput, 0, sizeof(pInput));
	  // Read input 
	  cin.getline(pInput, 256);
	  
	  //validate commands entered according to current state of client
	  if (strcmp(pInput,"exit")==0 || strcmp(pInput,"exit")==0){
			//exit cmd, terminate 
			cout << "[Application] Terminating." << endl;
			exit(1);
	  }
	  else if (loggedIn){
		if (validateInput(pInput)==0){
			cout<<"[Application] Invalid Command! Try Again.."<<endl;
			continue;
		}
		else if (validateInput(pInput)==2){
			//client is requesting a picture, call picture query function
			queryPicture(iSocket, pInput);
			continue;
		}
		else if (validateInput(pInput)==3){
			//client is update a picture, call picture send function
			sendPicture(iSocket, pInput);
			continue;
		}
		else if (validateInput(pInput)==5){
			continue;
		}
	  }
	  else {
		//not logged in
		if (validateLogin(pInput)==0){
			cout<<"[Application] Invalid Command! Try Again.."<<endl;
			continue;
		}
		else if (validateLogin(pInput)==1){
			loginattempt=1;
		}
		else if (validateLogin(pInput)==5){
			continue;
		}
	  }

 
	  
	  int iDataLength = strlen(pInput)+1;
	  
	  cout << "[Application] Sending: " << pInput << endl;
	  
	  // Block until data is sent to network
	  if ( ( iSendLength = ap_to_nw_send( iSocket, pInput, iDataLength ) ) != iDataLength ) {
	    cout << "[Application] Error sending data to network." << endl;
	    exit(1);
	  }
	  cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
	  
	  char * pReceivedData;
	  
	  // Block until data is received from network
	  if ( ( iRecvLength = nw_to_ap_recv( iSocket, &pReceivedData ) ) == -1 ) {
	    cout << "[Application] Error receiving data from network." << endl;
	    exit(1);
	  }
	  cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
	  cout << "[Application] Received: " << pReceivedData << endl;
          
	  char replyMsg[iRecvLength];
	  memset(replyMsg, 0, iRecvLength);
	  memcpy(replyMsg, getServerReply(pReceivedData), iRecvLength);

	  if (pReceivedData[0] == '1' && loginattempt){
		cout<<"[Application] Logged in successfully. "<<endl;
		loggedIn = 1;
		}
	  else if (pReceivedData[0] == '1')
		cout<<"[Application] "<<replyMsg<<endl;
	  else 
		cout<<"[Application] Command failed: "<<replyMsg<<endl;
	
	 
 }
 

  
  cout << "[Application] Terminating." << endl;
  pthread_exit(NULL);
}


/* Function takes a QUERY PICTURE command 
   sends it to network layer, and receives picture if available.   
   Picture is saved in './pictures' folder */
void queryPicture(intptr_t sockt, char* pInput){
	
	int iRecvLength; // length of recieved data
  	int iSendLength; // length of sent data  

	int pictureFile;
        char *pictureBuffer;	//buffer for picture data

	  int iDataLength = strlen(pInput)+1;
	  
	  cout << "[Application] Sending: " << pInput << endl;
	  
	  // Block until data is sent to network
	  // Send command first
	  if ( ( iSendLength = ap_to_nw_send( sockt, pInput, iDataLength ) ) != iDataLength ) {
	    cout << "[Application] Error sending data to network." << endl;
	    exit(1);
	  }
	  cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
	  
	  char * pReceivedData;
	  
	  // Block until data is received from network
	  // Receive reply from server
	  if ( ( iRecvLength = nw_to_ap_recv( sockt, &pReceivedData ) ) == -1 ) {
	    cout << "[Application] Error receiving data from network." << endl;
	    exit(1);
	  }
	  cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
	  cout << "[Application] Received: " << pReceivedData << endl;

	  //check if reply msg is success
	  if (pReceivedData[0]==1){
		  char filename[80];
		  //path to be saved
		  strcpy(filename, "./pictures/");
		  strcat(filename,getServerReply(pReceivedData));

		  //open picture file
		  pictureFile = open(filename,  O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);

		  // Now receive picture data
		  if ( ( iRecvLength = nw_to_ap_recv( sockt, &pictureBuffer ) ) == -1 ) {
		    cout << "[Application] Error receiving data from network." << endl;
		    exit(1);
		  }
		  cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
		  cout << "[Application] Received: " << pReceivedData << endl;

		  //write received data to file
		  write(pictureFile,pictureBuffer,iRecvLength);
		  //close image file
		  close(pictureFile); 
	  }
	  else {
		cout<<"[Application] Error: "<<getServerReply(pReceivedData)<<endl;
		
	  }

}

/* Function takes an UPDATE PICTURE <filename> command 
   sends it to network layer along with the provided picture */
void sendPicture(intptr_t sockt, char* pInput){

	int iRecvLength; // length of received data
  	int iSendLength; // length of sent data

	int pictureFile;	
        char *pictureBuffer;	//buffer for picture data
	struct stat fileStats;

	char filename[80];
	//image location
	strcpy(filename, "./");
	strcat(filename,getFilename(pInput));

	int iDataLength = strlen(pInput)+1;


	pictureFile = open(filename, O_RDONLY);
	if (pictureFile == -1) {
		cout << "[Application] Error: Unable to open "<<filename<<endl;
		exit(-1);
	}

	//get the size of the file to be sent 
	fstat(pictureFile, &fileStats);
	unsigned int fileSize = (unsigned int)fileStats.st_size;

	if (fileSize > 1024*1000*2){
		cout<<"[Application] Error: Filesize greater than 2MB!"<<endl;
	}

	//read file contents to picture buffer
	if (read(pictureFile,  pictureBuffer,  fileSize) == -1){
		cout << "[Application] Error: Unable to read file "<<filename<<endl;
		exit(-1);
	}

	cout << "[Application] Sending: " << pInput << endl;
	  
	  // Block until data is sent to network
	  // Send command and filename
	  if ( ( iSendLength = ap_to_nw_send( sockt, pInput, iDataLength ) ) != iDataLength ) {
	    cout << "[Application] Error sending data to network." << endl;
	    exit(1);
	  }

	  cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;

	
	  // Block until data is sent to network
	  // Send picture data
	  if ( ( iSendLength = ap_to_nw_send( sockt, pictureBuffer, fileSize ) ) != fileSize ) {
	    cout << "[Application] Error sending data to network." << endl;
	    exit(1);
	  }
	  cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;

	  
	


	  
	  char * pReceivedData;
	  
	  // Block until data is received from network
	  // get reply from server
	  if ( ( iRecvLength = nw_to_ap_recv( sockt, &pReceivedData ) ) == -1 ) {
	    cout << "[Application] Error receiving data from network." << endl;
	    exit(1);
	  }
	  
	  
	  cout << "[Application] Received: " << iRecvLength << " byte data from network." << endl;
	  cout << "[Application] Received: " << pReceivedData << endl;
	  //check if picture upload was successful
	  if (pReceivedData[0] == 1)
		cout<<"[Application] Picture updated successfully."<<endl;
	  else 
		cout<<"[Application] Picture upload failed with message: "<<getServerReply(pReceivedData)<<endl;
	

}


