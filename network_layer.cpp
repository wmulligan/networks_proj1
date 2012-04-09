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
#include <stdint.h>
#include <string.h>

#include "network_layer.h"
#include "queue.h"

#define PACKET_SIZE 256
#define PACKET_HDR_LENGTH 3

using namespace std;

struct sPacket {
  uint16_t iNumber; // 2 btye sequence number
  uint8_t iEnd; // 1 byte end of packet
  char pPayload[PACKET_SIZE]; // payload pointer
};

int iSequenceCounter = 0; // sequence counter

// prototypes
void * ApToDlHandler( void * longPointer );
void * DlToApHandler( void * longPointer );

void * NetworkLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  pthread_t iApToDlThreadId; // physical layer receive thread id
  pthread_t iDlToApThreadId; // physical layer send thread id
  
  cout << "[Network] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iApToDlThreadId, NULL, &ApToDlHandler, (void *) iSocket);
  pthread_create(&iDlToApThreadId, NULL, &DlToApHandler, (void *) iSocket);
  pthread_join(iApToDlThreadId, NULL);
  pthread_join(iDlToApThreadId, NULL);
  
  cout << "[Network] Terminating." << endl;
  pthread_exit(NULL);
}

void * ApToDlHandler( void * longPointer )
{
  int iSocket = (int) longPointer; // client socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  char * pData; // data pointer
  char * pDataCopy; // data pointer copy
  sPacket * pPacket; // packet pointer
  int iPacketLength; // packet length
  
  while ( true )
  {
    // Block until data is received from application
    if ( ( iRecvLength = ap_to_nw_recv( iSocket, &pData ) ) == -1 ) {
      cout << "[Network] Error receiving data from application." << endl;
      pthread_exit(NULL);
    }
    cout << "[Network] Received " << iRecvLength << " byte data from application." << endl;
    pDataCopy = pData;
    
    // Break data into packets
    while ( iRecvLength > 0 )
    {
      // New packet
      pPacket = new sPacket();
      pPacket->iNumber = iSequenceCounter++;
      pPacket->iEnd = ((iRecvLength > PACKET_SIZE) ? 0x00 : 0x01);
      memcpy( pPacket->pPayload, pData, ((iRecvLength > PACKET_SIZE) ? PACKET_SIZE : iRecvLength) );
      iPacketLength = PACKET_HDR_LENGTH + ((iRecvLength > PACKET_SIZE) ? PACKET_SIZE : iRecvLength);

      cout << "[Network] Sending: " << pPacket->iNumber << "|" << static_cast<int>(pPacket->iEnd) << "|" << pPacket->pPayload << endl;

      // Block until packet is sent to datalink
      if ( ( iSendLength = nw_to_dl_send( iSocket, (char *) pPacket, iPacketLength ) ) != iPacketLength ) {
        cout << "[Network] Error sending packet to datalink." << endl;
        pthread_exit(NULL);
      }
      cout << "[Network] Sent " << iSendLength << " byte packet to datalink." << endl;
      
      pData += PACKET_SIZE;
      iRecvLength -= PACKET_SIZE;
    }
    delete pDataCopy;
  }
}

void * DlToApHandler( void * longPointer )
{
  int iSocket = (int) longPointer; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data
  sPacket * pPacket; // packet pointer
  char * pData; // data pointer
  int iDataLength; // data length

  while ( true )
  {
    iDataLength = 0;
    pData = NULL;
    
    // combine packets into data
    while ( true )
    {
      // Block until packet is received from datalink
      if ( ( iRecvLength = dl_to_nw_recv( iSocket, (char**) &pPacket ) ) == -1 ) {
        cout << "[Network] Error receiving packet from datalink." << endl;
        pthread_exit(NULL);
      }
      cout << "[Network] Received " << iRecvLength << " byte packet from datalink." << endl;
      cout << "[Network] Received: " << pPacket->iNumber << "|" << static_cast<int>(pPacket->iEnd) << "|" << pPacket->pPayload << endl;

      // Allocate more memory
      iDataLength += (iRecvLength - PACKET_HDR_LENGTH);
      pData = (char *) realloc( pData, iDataLength );
      
      // Copy packet data into data
      memcpy( pData + iDataLength - (iRecvLength - PACKET_HDR_LENGTH), pPacket->pPayload, iRecvLength - PACKET_HDR_LENGTH );
      
      if (pPacket->iEnd == 0x01) {
        delete pPacket;
        break;
      }
      delete pPacket;
    }
    
    // Block until data is sent to application
    if ( ( iSendLength = nw_to_ap_send( iSocket, pData, iDataLength ) ) != iDataLength ) {
      cout << "[Network] Error sending data to application." << endl;
      pthread_exit(NULL);
    }
    cout << "[Network] Sent " << iSendLength << " byte data to application." << endl;
  }
}

