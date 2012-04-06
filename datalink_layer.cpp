/* datalink_layer.cpp
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

#include "datalink_layer.h"
#include "queue.h"

using namespace std;

// prototypes
void * NwToPhHandler( void * longPointer );
void * PhToNwHandler( void * longPointer );

void * DataLinkLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  pthread_t iNwToPhThreadId; // physical layer receive thread id
  pthread_t iPhToNwThreadId; // physical layer send thread id
  
  cout << "[DataLink] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iNwToPhThreadId, NULL, &NwToPhHandler, (void *) iSocket);
  pthread_create(&iPhToNwThreadId, NULL, &PhToNwHandler, (void *) iSocket);
  pthread_join(iNwToPhThreadId, NULL);
  pthread_join(iPhToNwThreadId, NULL);
  
  cout << "[DataLink] Terminating." << endl;
}

void * NwToPhHandler( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  
  while ( true )
  {
    char * pPacket; // packet pointer
    
    // Block until packet is received from network
    if ( ( iRecvLength = nw_to_dl_recv( iSocket, &pPacket ) ) == -1 ) {
      cout << "[DataLink] Error receiving packet from network." << endl;
      break;
    }
    cout << "[DataLink] Received " << iRecvLength << " byte packet from network." << endl;
    
    /* Turn packet into frame here */
    char * pFrame = pPacket;
    int iFrameLength = iRecvLength;
    
    // Block until frame is sent to physical
    if ( ( iSendLength = dl_to_ph_send( iSocket, pFrame, iFrameLength ) ) != iFrameLength ) {
      cout << "[DataLink] Error sending frame to physical." << endl;
      break;
    }
    cout << "[DataLink] Sent " << iSendLength << " byte frame to physical." << endl;
  }
}

void * PhToNwHandler( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data

  while ( true )
  {
    char * pFrame; // frame pointer
    
    // Block until frame is received from physical
    if ( ( iRecvLength = ph_to_dl_recv( iSocket, &pFrame ) ) == -1 ) {
      cout << "[DataLink] Error receiving frame from physical." << endl;
      break;
    }
    cout << "[DataLink] Received " << iRecvLength << " byte frame from physical." << endl;
    
    /* Turn frame into packet here */
    char * pPacket = pFrame;
    int iPacketLength = iRecvLength;
    
    // Block until packet is sent to network
    if ( ( iSendLength = dl_to_nw_send( iSocket, pPacket, iPacketLength ) ) != iPacketLength ) {
      cout << "[DataLink] Error sending packet to network." << endl;
      break;
    }
    cout << "[DataLink] Sent " << iSendLength << " byte packet to network." << endl;
  }
}

