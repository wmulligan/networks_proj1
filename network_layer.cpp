/* network_layer.cpp
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

#include "network_layer.h"
#include "queue.h"

using namespace std;

// prototypes
void * ApToDlHandler( void * longPointer );
void * DlToApHandler( void * longPointer );

void * NetworkLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  pthread_t iApToDlThreadId; // physical layer receive thread id
  pthread_t iDlToApThreadId; // physical layer send thread id
  
  cout << "[Network] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iApToDlThreadId, NULL, &ApToDlHandler, (void *) iSocket);
  pthread_create(&iDlToApThreadId, NULL, &DlToApHandler, (void *) iSocket);
  pthread_join(iApToDlThreadId, NULL);
  pthread_join(iDlToApThreadId, NULL);
  
  cout << "[Network] Terminating." << endl;
}

void * ApToDlHandler( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  
  while ( true )
  {
    char * pData; // data pointer
    
    // Block until data is received from application
    if ( ( iRecvLength = ap_to_nw_recv( iSocket, &pData ) ) == -1 ) {
      cout << "[Network] Error receiving data from application." << endl;
      break;
    }
    cout << "[Network] Received " << iRecvLength << " byte data from application." << endl;
    
    /* Turn data into packet here */
    char * pPacket = pData;
    int iPacketLength = iRecvLength;
    
    // Block until packet is sent to datalink
    if ( ( iSendLength = nw_to_dl_send( iSocket, pPacket, iPacketLength ) ) != iPacketLength ) {
      cout << "[Network] Error sending packet to datalink." << endl;
      break;
    }
    cout << "[Network] Sent " << iSendLength << " byte packet to datalink." << endl;
  }
}

void * DlToApHandler( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data

  while ( true )
  {
    char * pPacket; // packet pointer
    
    // Block until packet is received from datalink
    if ( ( iRecvLength = dl_to_nw_recv( iSocket, &pPacket ) ) == -1 ) {
      cout << "[Network] Error receiving packet from datalink." << endl;
      break;
    }
    cout << "[Network] Received " << iRecvLength << " byte packet from datalink." << endl;
    
    /* Turn packet into data here */
    char * pData = pPacket;
    int iDataLength = iRecvLength;
    
    // Block until data is sent to application
    if ( ( iSendLength = nw_to_ap_send( iSocket, pData, iDataLength ) ) != iDataLength ) {
      cout << "[Network] Error sending data to application." << endl;
      break;
    }
    cout << "[Network] Sent " << iSendLength << " byte data to application." << endl;
  }
}

