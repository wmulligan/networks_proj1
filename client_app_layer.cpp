/* client_app_layer.cpp
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

#include "client_app_layer.h"
#include "queue.h"

using namespace std;

void * ApplicationLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  
  cout << "[Application] Initializing..." << endl;
  
  while (1) {
  
    char pData[256];
    cin.getline(pData, 256);
    int iDataLength = strlen(pData)+1;
    
    cout << "[Application] Sending: " << pData << endl;
    
    // Block until data is sent to network
    if ( ( iSendLength = ap_to_nw_send( iSocket, pData, iDataLength ) ) != iDataLength ) {
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
  
  }
  
  cout << "[Application] Terminating." << endl;
}

