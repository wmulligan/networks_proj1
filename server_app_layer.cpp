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

#include "server_app_layer.h"
#include "queue.h"

using namespace std;

void * ApplicationLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  
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
    
    char pSendData[] = "World!";
    int iDataLength = sizeof(pSendData);
    
    cout << "[Application] Sending: " << pSendData << endl;
    
    // Block until data is sent to network
    if ( ( iSendLength = ap_to_nw_send( iSocket, pSendData, iDataLength ) ) != iDataLength ) {
      cout << "[Application] Error sending data to network." << endl;
      exit(1);
    }
    cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
  }
  
  cout << "[Application] Terminating." << endl;
}

