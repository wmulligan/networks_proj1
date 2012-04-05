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
uint8_t sendAck(uint16_t seqNumber, struct linkLayerSync *syncInfo);
uint8_t transmitFrame(struct frameInfo *frame, struct linkLayerSync *syncInfo);
void handleAck(struct frameInfo *frame, struct linkLayerSync *syncInfo);
uint8_t disassembleFrame(struct frameInfo *frame, uint8_t *received, int receivedLen);

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
    
    // Block until packet is received from network
    if ( ( iRecvLength = nw_to_dl_recv( iSocket, &pPacket ) ) == -1 ) {
      cout << "[DataLink] Error receiving packet from network." << endl;
      break;
    }
    cout << "[DataLink] Received " << iRecvLength << " byte packet from network." << endl;
    
    if(iRecvLength > MAX_PAYLOAD_SIZE) {
      // Handle packets larger than maximum allowable payload size here
      frameToSend = (struct frameInfo *)malloc(sizeof(struct frameInfo));

      // Send first part of packet
      frameToSend->frameType = 0x00;
      frameToSend->endOfPacket = 0x00;
      frameToSend->payloadLength = MAX_PAYLOAD_SIZE;      

      // Copy in the payload
      for(int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
	frameToSend->payload[i] = pPacket[i];
      }

      // Checksum and sequence number are managed by transmitFrame
      transmitFrame(frameToSend, syncInfo);

      // Now set up the second part of the packet and send it
      frameToSend->frameType = 0x00;
      frameToSend->endOfPacket = 0x01;
      frameToSend->payloadLength = iRecvLength - MAX_PAYLOAD_SIZE;      

      for(int i = MAX_PAYLOAD_SIZE; i < iRecvLength; i++) {
	frameToSend->payload[i - MAX_PAYLOAD_SIZE] = pPacket[i];
      }

      // Checksum and sequence number are managed by transmitFrame
      transmitFrame(frameToSend, syncInfo);

      free(frameToSend);
    } else {
      frameToSend = (struct frameInfo *)malloc(sizeof(struct frameInfo));

      frameToSend->frameType = 0x00;
      frameToSend->endOfPacket = 0x01;
      frameToSend->payloadLength = iRecvLength;
      
      // Copy in the payload
      for(int i = 0; i < iRecvLength; i++) {
	frameToSend->payload[i] = pPacket[i];
      }

      // Checksum and sequence number are managed by transmitFrame
      transmitFrame(frameToSend, syncInfo);

      free(frameToSend);
    }
  }
}

void * PhToNwHandler( void * longPointer )
{
  int iSocket; // client socket handle
  int iRecvLength; // length of received data
  int iSendLength; // length of sent data
  char * pPacket;
  char stash[MAX_PAYLOAD_SIZE];
  int stashReady = 0;
  int iPacketLength;  
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)longPointer;
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

    receivedFrame = (struct frameInfo *)malloc(sizeof(struct frameInfo));

    // Disassemble the frame
    if(1 == disassembleFrame(receivedFrame, pFrame, iRecvLength)) {
      cout << "[DataLink] Received invalid frame, ignoring!" << endl;
    }

    // Check if we received an ACK
    if(receivedFrame->frameType == 1) {
      handleAck(receivedFrame, syncInfo);
      free(receivedFrame);
      continue;
    } else if(receivedFrame->frameType != 0) {
      // Bad frame type field
      cout << "[DataLink] Received frame with bad Frame Type field!" << endl;
      free(receivedFrame);
      continue;
    }

    // Check sequence number. See if it's what we were expecting.
    if(receivedFrame->seqNumber != syncInfo->recvSequence) {
      cout << "[DataLink] Received out of sequence frame (expected sequence number "
	   << syncInfo->recvSequence << ", received " << receivedFrame->seqNumber << ")." << endl;
      free(receivedFrame);
      continue;
    } else {
      // Good sequence number, increment next expected received packet
      // Critical Section
      pthread_spin_lock(&(syncInfo->lock));
      syncInfo->recvSequence++; // Increment expected packet to receive
      pthread_spin_unlock(&(syncInfo->lock));
      // End Critical Section
    }

    // Block until ack is sent to physical
    sendAck(receivedFrame->seqNumber, syncInfo);

    // Ack has been sent. Extract contents of frame and forward them up to network layer.
    if(receivedFrame->endOfPacket != 1) {
      if(receivedFrame->payloadLength != MAX_PAYLOAD_SIZE) {
	cout << "Frame with invalid size and no end-of-packet marker received!" << endl;
	free(receivedFrame);
	continue; // Bad frame!
      }
      for(int i = 0; i < receivedFrame->payloadLength; i++) {
	stash[i] = receivedFrame->payload[i];
	stashReady = 1;
      }
    } else {
      // Built on the assumption that we receive no more than 2 packets in a row without endOfFrame set.
      if(stashReady = 1) {
	stashReady = 0;
	iPacketLength = receivedFrame->payloadLength + MAX_PAYLOAD_SIZE;
	pPacket = (char *)malloc(iPacketLength);
	for(int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
	  pPacket[i] = stash[i];
	}
	for(int i = MAX_PAYLOAD_SIZE; i < iPacketLength; i++) {
	  pPacket[i] = receivedFrame->payload[i - MAX_PAYLOAD_SIZE];
	}
      } else {
	iPacketLength = receivedFrame->payloadLength;
	pPacket = (char *)malloc(iPacketLength);
	for(int i = 0; i < iPacketLength; i++) {
	  pPacket[i] = receivedFrame->payload[i];
	}
      }
    }
    
    // Block until packet is sent to network
    if ( ( iSendLength = dl_to_nw_send( iSocket, pPacket, iPacketLength ) ) != iPacketLength ) {
      cout << "[DataLink] Error sending packet to network." << endl;
      free(receivedFrame);
      free(pPacket);
      continue;
    }

    free(receivedFrame);
    free(pPacket);
    cout << "[DataLink] Sent " << iSendLength << " byte packet to network." << endl;
  }
}

/**
 * Create an appropriate frame from a frameInfo struct and transmit it on the wire.
 *
 * @return Number of bytes transmitted
 */
uint8_t transmitFrame(struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  uint8_t *buffer;
  uint16_t checksum;
  int i, toReturn = 0;

  if(frame == 0) {
    cout << "Invalid frame passed to transmitFrame! Ignoring!" << endl;
    return 0x00;
  }

  // Set sequence number for this frame
  frame->seqNumber = syncInfo->sendSequence;
  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->sendSequence++; // Increment sequence number for next sent frame
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section

  // Allocate a buffer to construct the frame in
  buffer = (uint8_t *)malloc(7 + frame->payloadLength);

  // We can copy in the first 5 bytes of the frameInfo struct, as they are static fields
  for(i = 0; i < 5; i++) {
    buffer[i] = ((uint8_t *)frame)[i];
  }

  // And now we can do the same with the payload
  for(i = 5; i < (5 + frame->payloadLength); i++) {
    buffer[i] = ((uint8_t *)frame)[i];
  }

  // Populate the checksum field
  checksum = generateFCS(frame);
  buffer[5 + frame->payloadLength] = (uint8_t)(checksum >> 8);
  buffer[6 + frame->payloadLength] = (uint8_t)(checksum & 0x00FF);

  // Wait for window to open up
  while(syncInfo->windowSize == 0);

  // Transmit our frame

  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->windowSize--; // Decrement available window slots
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, buffer, (7 + frame->payloadLength)) ) != (7 + frame->payloadLength)) {
    cout << "[DataLink] Error sending frame to physical." << endl;
  }
  cout << "[DataLink] Sent " << (7 + frame->payloadLength) << " byte frame to physical with sequence number " << frame->seqNumber << endl;

  free(buffer);

  return toReturn;
}

/**
 * Send an ACK frame for a specified sequence number
 */
uint8_t sendAck(uint16_t seqNumber, struct linkLayerSync *syncInfo)
{
  uint8_t *ack;
  int toReturn = 0;

  ack = (uint8_t *)malloc(ACK_SIZE); // ACK frames are constant size

  ack[0] = 0x01; // First byte is 1, indicating an ACK
  ack[1] = (uint8_t)(seqNumber >> 8); // First 8 bits of sequence number
  ack[2] = (uint8_t)(seqNumber & 0x00FF); // Next 8 bits of sequence number
  ack[3] = 0; // This field ignored
  ack[4] = 0; // This field ignored
  ack[5] = ack[1]; // FCS field matches sequence number
  ack[6] = ack[2];

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, ack, ACK_SIZE) ) != ACK_SIZE) {
    cout << "[DataLink] Error sending ACK to physical." << endl;
  }
  cout << "[DataLink] Sent ACK frame to physical with sequence number " << seqNumber << endl;

  free(ack);

  return toReturn;
}

/**
 * Disassemble a received frame to populate frameInfo struct.
 *
 * Returns 0 if valid frame (checksum match), 1 if invalid.
 */
uint8_t disassembleFrame(struct frameInfo *frame, uint8_t *received, int receivedLen)
{
  int i;

  if(frame == 0 || received == 0) {
    return 1;
  } else if(receivedLen < ACK_SIZE || receivedLen > MAX_FRAME_SIZE) {
    cout << "[DataLink] Received frame with bad size (over 150 or under 7)!" << endl;
    return 1;
  }

  frame->frameType = received[0];
  frame->seqNumber = ((uint16_t)(received[1]) << 8) + received[2];
  frame->endOfPacket = received[3];
  frame->payloadLength = received[4];

  if(frame->payloadLength > MAX_PAYLOAD_SIZE) {
    cout << "[DataLink] Oversized payload specified by received frame!" << endl;
    return 1;
  }

  for(i = 5; i <(5 + frame->payloadLength); i++) {
    frame->payload[i-5] = received[i];
  }

  // Verify checksum for data frames
  if(frame->frameType == 0 && generateFCS(frame) != ((uint16_t)(received[receivedLen - 2]) << 8) + received[receivedLen - 1]) {
    cout << "[DataLink] Received frame with bad FCS!" << endl;
    return 1;
  } else if(frame->frameType == 1 && (received[1] != received[5] || received[2] != received[6])) { // Same for ACK frames
    cout << "[DataLink] Received ACK with bad FCS!" << endl;
    return 1;
  }

  return 0;
}

/**
 * Take action based on an ACK frame received - including checking for validity
 */
void handleAck(struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  if(frame->seqNumber != syncInfo->ackSequence) {
    return;
  } else {
    // Critical Section
    pthread_spin_lock(&(syncInfo->lock));
    syncInfo->ackSequence++; // Increment next frame we expect an ACK for
    syncInfo->windowSize++; // Increment available window slots
    pthread_spin_unlock(&(syncInfo->lock));
    // End Critical Section    
  }
}

/**
 * Generate Frame Check Sequence for a frame from its frameInfo struct
 */
uint16_t generateFCS(struct frameInfo *frame)
{
  uint16_t checksum = 0x0000;
  uint8_t payloadLength = frame->payloadLength;
  uint8_t pos = 3;

  // Null check
  if(frame == 0) {
    return 0x00;
  }

  // Add starting values
  checksum += ((uint16_t *)frame)[0];

  // Fold on static fields
  checksum ^= ((uint16_t *)frame)[1];
  if(payloadLength > 0) {
    checksum ^= ((uint16_t *)frame)[2];
    payloadLength--;
  } else {
    checksum ^= ((uint16_t *)frame)[2] & 0xFF00;
  }

  // While we have over 2 bytes of payload left to fold in, fold in 16 bit quantities
  while(payloadLength > 2) {
    checksum ^= ((uint16_t *)frame)[pos];
    pos++;
    payloadLength -= 2;
  }

  // If we have a byte left over, fold it in.
  if(payloadLength == 1) {
    checksum ^= ((uint16_t *)frame)[pos] & 0xFF00; 
  }

  return checksum;
}
