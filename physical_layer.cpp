/**
 * Sends and recvs bytes over TCP
 *
 * @file physical_layer.cpp
 * @author Will Mulligan
 */
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "physical_layer.h"
#include "queue.h"
#include "global.h"

#define SLOT_SIZE 200
#define SLOT_HDR_LENGTH 1

using namespace std;

// prototypes
void *TcpToDlHandler( void *longPointer );
void *DlToTcpHandler( void *longPointer );

void *PhysicalLayer( void *longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  pthread_t iTcpToDlThreadId; // physical layer receive thread id
  pthread_t iDlToTcpThreadId; // physical layer send thread id
  
  cout << "[Physical] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iTcpToDlThreadId, NULL, &TcpToDlHandler, (void *) iSocket);
  pthread_create(&iDlToTcpThreadId, NULL, &DlToTcpHandler, (void *) iSocket);
  pthread_join(iTcpToDlThreadId, NULL);
  pthread_join(iDlToTcpThreadId, NULL);
  
  cout << "[Physical] Terminating." << endl;
  pthread_exit(NULL);
}

void *TcpToDlHandler( void *longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  
  char * pFrame; // frame pointer
  char * pSlot;
  char * pSlotCopy;
  int iSlotLength;
  
  while ( true )
  {
    pSlot = (char *) malloc(sizeof(char)*1024);
    
    // Block until frame is received from tcp
    while ( ( iRecvLength = recv( iSocket, pSlot, 1024, NULL ) ) <= 0 ) {
      if (errno == EAGAIN) { usleep(1); }
      else {
        cout << "[Physical] Error receiving frame from tcp." << endl;
        terminateQueue( iSocket );
        pthread_exit(NULL);
      }

    }
    if (g_debug) cout << "[Physical] Received " << iRecvLength << " bytes from tcp." << endl;
    pSlotCopy = pSlot;
    
    while ( iRecvLength > 0 ) {
      iSlotLength = (unsigned char)pSlot[0];
      pFrame = (char *) malloc(sizeof(char)*iSlotLength);
      memcpy( pFrame, pSlot+1, iSlotLength );
      iRecvLength -= iSlotLength+1;
      pSlot += iSlotLength+1;
      
      // Block until frame is sent to datalink
      if ( ( iSendLength = ph_to_dl_send( iSocket, pFrame, iSlotLength ) ) != iSlotLength ) {
        cout << "[Physical] Error sending frame to datalink." << endl;
        pthread_exit(NULL);
      }
      if (g_debug) cout << "[Physical] Sent " << iSendLength << " byte frame to datalink." << endl;
    }
    delete pSlotCopy;
  }
}

void *DlToTcpHandler( void *longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data
  char * pFrame; // frame pointer
  char * pSlot;
  int iSlotLength;

  while ( true )
  {
    // Block until frame is received from datalink
    if ( ( iRecvLength = dl_to_ph_recv( iSocket, &pFrame ) ) == -1 ) {
      cout << "[Physical] Error receiving frame from datalink." << endl;
      pthread_exit(NULL);
    }
    if (g_debug) cout << "[Physical] Received " << iRecvLength << " byte frame from datalink." << endl;
    
    pSlot = (char *) malloc(sizeof(char)*(SLOT_SIZE+SLOT_HDR_LENGTH));
    pSlot[0] = (unsigned char) iRecvLength;
    memcpy( pSlot+1, pFrame, iRecvLength );
    iSlotLength = iRecvLength + SLOT_HDR_LENGTH;
    delete pFrame;
    
    // Block until data is sent to tcp
    if ( ( iSendLength = send( iSocket, (char *) pSlot, iSlotLength, NULL ) ) != iSlotLength ) {
      cout << "[Physical] Error sending frame to tcp." << endl;
      pthread_exit(NULL);
    }
    if (g_debug) cout << "[Physical] Sent " << iSendLength << " bytes to tcp." << endl;
    
    delete pSlot;
  }
}




