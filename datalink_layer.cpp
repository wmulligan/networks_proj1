/**
 * Contains datalink layer functionality of protocol
 *
 * @file datalink_layer.cpp
 * @author Will Mulligan and Matthew Heon
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
uint16_t generateFCS(struct frameInfo *frame);

void * DataLinkLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  pthread_t iNwToPhThreadId; // physical layer receive thread id
  pthread_t iPhToNwThreadId; // physical layer send thread id
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)malloc(sizeof(struct linkLayerSync));

  // Populate the syncronization structure assigned to the threads
  syncInfo->socket = iSocket;
  syncInfo->windowSize = WINDOW_SIZE;
  syncInfo->sendSequence = 0;
  syncInfo->recvSequence = 0;
  syncInfo->ackSequence = 0;
  // Initialize the spinlock for syncronization
  pthread_spin_init(&(syncInfo->lock), 0);
  
  cout << "[DataLink] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iNwToPhThreadId, NULL, &NwToPhHandler, (void *) syncInfo);
  pthread_create(&iPhToNwThreadId, NULL, &PhToNwHandler, (void *) syncInfo);
  pthread_join(iNwToPhThreadId, NULL);
  pthread_join(iPhToNwThreadId, NULL);
  
  // Free the sync structure
  free(syncInfo);

  cout << "[DataLink] Terminating." << endl;
}

void * NwToPhHandler( void * longPointer )
{
  char *pFrame;
  int iFrameLength;
  int iSocket; // client socket handle
  int iSendLength; // Length of send data
  int iRecvLength; // length of recieved data
  struct frameInfo *frameToSend;
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)longPointer;

  iSocket = syncInfo->socket;

  while ( true )
  {
    char * pPacket; // packet pointer
    
    // Null out fields used in transmission so our null checks are guaranteed to fail on bad frames
    pFrame = 0;
    iFrameLength = 0;

    // Block until packet is received from network
    if ( ( iRecvLength = nw_to_dl_recv( iSocket, &pPacket ) ) == -1 ) {
      cout << "[DataLink] Error receiving packet from network." << endl;
      break;
    }
    cout << "[DataLink] Received " << iRecvLength << " byte packet from network." << endl;
    
    if(iRecvLength > MAX_PAYLOAD_SIZE) {
      // Handle packets larger than maximum allowable payload size here
    } else {
      frameToSend = (struct frameInfo *)malloc(sizeof(struct frameInfo));
      frameToSend->frameType = 0x00;

      // Critical section
      pthread_spin_lock(&(syncInfo->lock));
      frameToSend->seqNumber = (syncInfo->sendSequence)++; // Assign and increment packet sequence number
      pthread_spin_unlock(&(syncInfo->lock));
      // End Critical Section

      frameToSend->endOfPacket = 0x01; // We are an end of packet frame
      frameToSend->payloadLength = iRecvLength;
      // Copy payload into frame
      for(uint8_t i = 0; i < iRecvLength; i++) {
	frameToSend->payload[i] = pPacket[i];
      }
      // Append the FCS
      frameToSend->frameCheckSequence = generateFCS(frameToSend);

      /* Cast the frameInfo struct into a frame for transmission, set transmission length */
      pFrame = (char *)frameToSend;
      iFrameLength = iRecvLength + 7; // 7 bytes of framing, so length of packet is 7 bytes over payload length.
    }

    // This is really gross, but works. Spin until our window size is positive.
    while(syncInfo->windowSize == 0);

    // Critical Section
    pthread_spin_lock(&(syncInfo->lock));
    syncInfo->windowSize--; // Decrement available window slots
    pthread_spin_unlock(&(syncInfo->lock));
    // End Critical Section

    // Block until frame is sent to physical
    if ( ( iSendLength = dl_to_ph_send( iSocket, pFrame, iFrameLength ) ) != iFrameLength ) {
      cout << "[DataLink] Error sending frame to physical." << endl;
      break;
    }
    cout << "[DataLink] Sent " << iFrameLength << " byte frame to physical." << endl;
  }
}

void * PhToNwHandler( void * longPointer )
{
  int iSocket; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data
  char * pPacket;
  int iPacketLength;  
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)longPointer;
  struct ackFrameInfo *ack; // This will hold constructed ACKs to be transmitted
  struct frameInfo *receivedFrame; // Holds frame received

  iSocket = syncInfo->socket;

  while ( true )
  {
    char * pFrame; // frame pointer
    
    // Block until frame is received from physical
    if ( ( iRecvLength = ph_to_dl_recv( iSocket, &pFrame ) ) == -1 ) {
      cout << "[DataLink] Error receiving frame from physical." << endl;
      continue;
    }
    cout << "[DataLink] Received " << iRecvLength << " byte frame from physical." << endl;
    
    // Cast frame into a frame_info
    receivedFrame = (struct frameInfo *)pFrame;

    // Check if it's over max frame size
    if(iRecvLength > MAX_FRAME_SIZE) {
      cout << "[DataLink] Received oversized frame, discarding!" << endl;
      continue;
    }

    // Check if we received an ACK
    if(receivedFrame->frameType == 1) {
      // Check if sequence number and FCS match. If not, bad ACK.
      if(receivedFrame->seqNumber != receivedFrame->frameCheckSequence) {
	cout << "[DataLink] Bad ACK frame received - FCS and sequence number do not match! Discarding." << endl;
	continue;
      }
      // Check if we received 7 bytes. If we received more, bad ACK.
      if(iRecvLength != ACK_SIZE) {
	cout << "[DataLink] Received ACK with bad length! Discarding." << endl;
	continue;
      }
      // Check if sequence number makes sense
      if(receivedFrame->seqNumber != syncInfo->ackSequence) {
	cout << "[DataLink] Received an out of sequence ACK frame (expected " << syncInfo->ackSequence << ", received" 
	     << receivedFrame->seqNumber << ")." << endl;
	continue;
      }
      // OK sequence number makes sense, adjust sliding window
      // Critical Section
      pthread_spin_lock(&(syncInfo->lock));
      if(syncInfo->windowSize < MAX_WINDOW_SIZE)
	syncInfo->windowSize++; // Increment available window slots
      syncInfo->ackSequence++; // Increment next expected sequence number
      pthread_spin_unlock(&(syncInfo->lock));
      // End Critical Section
      continue;
    } else if(receivedFrame->frameType != 0) {
      // Bad frame type field
      cout << "[DataLink] Received frame with bad Frame Type field!" << endl;
      continue;
    }

    // We are now sure we received a data frame. So check its FCS for validity.
    uint16_t computedFCS = generateFCS(receivedFrame);
    if(computedFCS != receivedFrame->frameCheckSequence) {
      cout << "[DataLink] Received data frame with bad FCS, discarding!" << endl;
      continue;
    }

    // Check sequence number. See if it's what we were expecting.
    if(receivedFrame->seqNumber != syncInfo->recvSequence) {
      cout << "[DataLink] Received out of sequence frame (expected sequence number "
	   << syncInfo->recvSequence << ", received " << receivedFrame->seqNumber << ")." << endl;
      continue;
    } else {
      // Good sequence number, increment next expected received packet
      // Critical Section
      pthread_spin_lock(&(syncInfo->lock));
      syncInfo->recvSequence++; // Increment expected packet to receive
      pthread_spin_unlock(&(syncInfo->lock));
      // End Critical Section
    }

    // As we have established the frame is valid in every way we can see, we now send an ACK for it.
    ack = (struct ackFrameInfo *)malloc(sizeof(struct ackFrameInfo));
    ack->frameType = 0x01;
    ack->seqNumber = receivedFrame->seqNumber;
    ack->seqNumberRepeat = receivedFrame->seqNumber;
    ack->ignored = 0x00;

    // Block until ack is sent to physical
    if ( ( iSendLength = dl_to_ph_send( iSocket, (char *)ack, ACK_SIZE ) ) != ACK_SIZE ) {
      cout << "[DataLink] Error sending ack to physical." << endl;
      break;
    }
    cout << "[DataLink] Sent ack to physical for sequence number " << receivedFrame->seqNumber << "." << endl;

    // Ack has been sent. Extract contents of frame and forward them up to network layer.
    if(receivedFrame->endOfPacket != 1) {
      // Handle >143 byte packets here
    } else {
      char * pPacket = (char *)receivedFrame + 5; // 5 bytes from start of frame to start of payload
      int iPacketLength = receivedFrame->payloadLength;
    }
    
    // Block until packet is sent to network
    if ( ( iSendLength = dl_to_nw_send( iSocket, pPacket, iPacketLength ) ) != iPacketLength ) {
      cout << "[DataLink] Error sending packet to network." << endl;
      continue;
    }
    cout << "[DataLink] Sent " << iSendLength << " byte packet to network." << endl;
  }
}

/**
 * Generate Frame Check Sequence for a frame from its frameInfo struct
 */
uint16_t generateFCS(struct frameInfo *frame)
{
  uint16_t checksum = 0x0000;
  uint8_t payloadLength = frame->length;
  uint8_t pos = 3;

  // Null check
  if(frame == 0) {
    return 0x00;
  }

  // Add starting values
  checksum += (uint16_t *)frame[0];

  // Fold on static fields
  checksum ^= (uint16_t *)frame[1];
  if(payloadLength > 0) {
    checksum ^= (uint16_t *)frame[2];
    payloadLength--;
  } else {
    checksum ^= (uint16_t *)frame[2] & 0xFF00;
  }

  // While we have over 2 bytes of payload left to fold in, fold in 16 bit quantities
  while(payloadLength > 2) {
    checksum ^= (uint16_t *)frame[pos];
    pos++;
    payloadLength -= 2;
  }

  // If we have a byte left over, fold it in.
  if(payloadLength == 1) {
    checksum ^= (uint16_t)frame[pos] & 0xFF00; 
  }

  return checksum;
}
