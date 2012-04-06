/* server_app_layer.cpp
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
#include <mysql.h>

#include "server_app_layer.h"
#include "server_func.cpp"
#include "mysqlh.h"
#include "queue.h"

using namespace std;


void * ApplicationLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  MYSQL* mysqlConn = dbConnect(); //connect to db and save handler

  cout << "[Application] Initializing..." << endl;
  
  while ( true )
  {
    char * pData;
    
    

    // Block until data is received from network
    if ( ( iRecvLength = nw_to_ap_recv( iSocket, &pData ) ) == -1 ) {
      cout << "[Application] Error receiving data from network." << endl;
      exit(1);
    }
    cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
    cout << "[Application] Received: " << pData << endl;
	
    char pSendData[1000];
    memset(pSendData, 0, sizeof(pSendData));
    if (userType==0){
	/*if (isPicture(pSendData)==1){

	continue;
	}
	else if (isPicture(pSendData)==2){

	continue;
	}*/
	strcpy(pSendData,processLogin(pData, mysqlConn));
	//if user has not been authenticated yet
    }
    else {
	//user has logged in
	strcpy(pSendData,processCommand(pData,mysqlConn));
    } 
   
    int iDataLength = strlen(pSendData);
    
    cout << "[Application] Sending: " << pSendData << endl;
    
    // Block until data is sent to network
    if ( ( iSendLength = ap_to_nw_send( iSocket, pSendData, iDataLength ) ) != iDataLength ) {
      cout << "[Application] Error sending data to network." << endl;
      exit(1);
    }
    cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
  }
  
  dbClose(mysqlConn); //close connection to db

  cout << "[Application] Terminating." << endl;
}

