/* physical_layer.cpp
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

#include "physical_layer.h"
#include "queue.h"

using namespace std;

// prototypes
void *TcpToDlHandler( void *longPointer );
void *DlToTcpHandler( void *longPointer );

void *PhysicalLayer( void *longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  pthread_t iTcpToDlThreadId; // physical layer receive thread id
  pthread_t iDlToTcpThreadId; // physical layer send thread id
  
  cout << "[Physical] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iTcpToDlThreadId, NULL, &TcpToDlHandler, (void *) iSocket);
  pthread_create(&iDlToTcpThreadId, NULL, &DlToTcpHandler, (void *) iSocket);
  pthread_join(iTcpToDlThreadId, NULL);
  pthread_join(iDlToTcpThreadId, NULL);
  
  cout << "[Physical] Terminating." << endl;
}

void *TcpToDlHandler( void *longPointer )
{
  int iSocket = (int) longPointer; // client socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  char * pFrame; // todo: fix size
  
  while ( true )
  {
    pFrame = (char *) malloc(sizeof(char)*300);
    
    // Block until frame is received from tcp
    if ( ( iRecvLength = recv( iSocket, pFrame, 300, NULL ) ) <= 0 ) {
      cout << "[Physical] Error receiving frame from tcp." << endl;
      break;
    }
    cout << "[Physical] Received " << iRecvLength << " byte frame from tcp." << endl;
    cout << "[Physical] Received: " << pFrame << endl;
    
    // Block until frame is sent to datalink
    if ( ( iSendLength = ph_to_dl_send( iSocket, pFrame, iRecvLength ) ) != iRecvLength ) {
      cout << "[Physical] Error sending frame to datalink." << endl;
      break;
    }
    cout << "[Physical] Sent " << iSendLength << " byte frame to datalink." << endl;
  }
}

void *DlToTcpHandler( void *longPointer )
{
  int iSocket = (int) longPointer; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data

  while ( true )
  {
    char * pFrame; // frame pointer
    
    // Block until frame is received from datalink
    if ( ( iRecvLength = dl_to_ph_recv( iSocket, &pFrame ) ) == -1 ) {
      cout << "[Physical] Error receiving frame from datalink." << endl;
      break;
    }
    cout << "[Physical] Received " << iRecvLength << " byte frame from datalink." << endl;
    cout << "[Physical] Received: " << pFrame << endl;
    
    // Block until data is sent to tcp
    if ( ( iSendLength = send( iSocket, pFrame, iRecvLength, NULL ) ) != iRecvLength ) {
      cout << "[Physical] Error sending frame to tcp." << endl;
      break;
    }
    cout << "[Physical] Sent " << iSendLength << " byte frame to tcp." << endl;
    
    //delete(&pFrame);
  }
}




