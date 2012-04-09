/* server_app_layer.cpp
 * Will Mulligan
 */
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

#include "server_app_layer.h"
#include "queue.h"

using namespace std;

void * ApplicationLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  char * pData; // data pointer
  int iDataLength;
  
  cout << "[Application] Initializing..." << endl;
  
  while ( true )
  {
    // Block until data is received from network
    if ( ( iRecvLength = nw_to_ap_recv( iSocket, &pData ) ) == -1 ) {
      cout << "[Application] Error receiving data from network." << endl;
      break;
    }
    cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
    cout << "[Application] Received: " << pData << endl;
    
    delete pData;
    
    pData = (char *) malloc(sizeof(char) * 256);
    strcpy(pData, "reply");
    iDataLength = strlen(pData) + 1;
    
    cout << "[Application] Sending: " << pData << endl;
    
    // Block until data is sent to network
    if ( ( iSendLength = ap_to_nw_send( iSocket, pData, iDataLength ) ) != iDataLength ) {
      cout << "[Application] Error sending data to network." << endl;
      break;
    }
    cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
  }
  
  cout << "[Application] Terminating." << endl;
  pthread_exit(NULL);
}

