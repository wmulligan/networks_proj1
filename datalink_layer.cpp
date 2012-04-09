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
#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "datalink_layer.h"
#include "queue.h"

using namespace std;

// prototypes
void * NwToPhHandler( void * longPointer );
void * PhToNwHandler( void * longPointer );
uint16_t generateFCS(uint8_t *frame, uint8_t length);
uint8_t sendAck(uint16_t seqNumber, struct linkLayerSync *syncInfo);
uint8_t transmitFrame(struct frameInfo *frame, struct linkLayerSync *syncInfo);
void handleAck(struct frameInfo *frame, struct linkLayerSync *syncInfo);
uint8_t disassembleFrame(struct frameInfo *frame, uint8_t *received, int receivedLen);
void handleRetransmission(uint16_t failedFrameSeq, struct linkLayerSync *syncInfo);
void armTimer(uint16_t seqNumber, struct linkLayerSync *syncInfo);

void * DataLinkLayer( void * longPointer )
{
  int iSocket = (int) longPointer; // socket handle
  pthread_t iNwToPhThreadId; // physical layer receive thread id
  pthread_t iPhToNwThreadId; // physical layer send thread id
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)malloc(sizeof(struct linkLayerSync));

  // Populate the syncronization structure assigned to the threads
  syncInfo->socket = iSocket;
  syncInfo->windowSize = WINDOW_SIZE;
  syncInfo->mainSequence = 0;
  syncInfo->ackSequence = 0;
  syncInfo->recentFramesIndex = 0;
  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    syncInfo->recentFrames[i].frame = 0;
  }
  // Initialize the spinlock for syncronization
  pthread_spin_init(&(syncInfo->lock), 0);

  // Create a timer for timeouts
  timer_create(CLOCK_REALTIME, NULL, &(syncInfo->timer));
  
  cout << "[DataLink] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iNwToPhThreadId, NULL, &NwToPhHandler, (void *) syncInfo);
  pthread_create(&iPhToNwThreadId, NULL, &PhToNwHandler, (void *) syncInfo);
  pthread_join(iNwToPhThreadId, NULL);
  pthread_join(iPhToNwThreadId, NULL);
  
  // Free the sync structure
  free(syncInfo);

  cout << "[DataLink] Terminating." << endl;
  pthread_exit(NULL);
}

void * NwToPhHandler( void * longPointer )
{
  char *pFrame;
  int iFrameLength;
  int iSocket; // client socket handle
  int iSendLength; // Length of send data
  int iRecvLength; // length of recieved data
  timer_t timerId;
  struct sigevent timerEvent;
  struct sigaction timerAction;
  struct frameInfo *frameToSend;
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)longPointer;

  iSocket = syncInfo->socket;

  // Set up our sigevent struct (used for realtime timer for go-back-n).
  timerEvent.sigev_notify = SIGEV_SIGNAL;
  timerEvent.sigev_signo = SIGUSR1;
  timerEvent.sigev_value.sival_ptr = &timerId;

  while ( true )
    {
      char * pPacket; // packet pointer
    
      // Block until packet is received from network
      if ( ( iRecvLength = nw_to_dl_recv( iSocket, &pPacket ) ) == -1 ) {
	cout << "[DataLink] Error receiving packet from network." << endl;
	pthread_exit(NULL);
      }
      cout << "[DataLink] Received " << iRecvLength << " byte packet from network." << endl;

#ifdef VERBOSE_IPC_DEBUG
      printf("[DataLink] Received string %s from Network Layer\n", pPacket);

#endif
    
      if(iRecvLength > MAX_PAYLOAD_SIZE) {
	if(iRecvLength >= 2 * MAX_PAYLOAD_SIZE) {
	  cout << "[DataLink] Got bad packet from network - too large to send!" << endl;
	  continue;
	}

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

	struct timeval curtime;
	gettimeofday(&curtime, NULL);

	struct frameInfo *temp = (struct frameInfo *)malloc(sizeof(struct frameInfo));
	memcpy(temp, frameToSend, sizeof(struct frameInfo));

	// Critical Section
	pthread_spin_lock(&(syncInfo->lock));
	syncInfo->recentFrames[syncInfo->recentFramesIndex].frame = temp;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_sec = curtime.tv_sec;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_usec = curtime.tv_usec;
	syncInfo->recentFramesIndex++;
	pthread_spin_unlock(&(syncInfo->lock));
	// End Critical Section	

	// Checksum and sequence number are managed by transmitFrame
	transmitFrame(frameToSend, syncInfo);

	// Now set up the second part of the packet and send it
	frameToSend->frameType = 0x00;
	frameToSend->endOfPacket = 0x01;
	frameToSend->payloadLength = iRecvLength - MAX_PAYLOAD_SIZE;      

	for(int i = MAX_PAYLOAD_SIZE; i < iRecvLength; i++) {
	  frameToSend->payload[i - MAX_PAYLOAD_SIZE] = pPacket[i];
	}

	gettimeofday(&curtime, NULL);

	temp = (struct frameInfo *)malloc(sizeof(struct frameInfo));
	memcpy(temp, frameToSend, sizeof(struct frameInfo));

	// Critical Section
	pthread_spin_lock(&(syncInfo->lock));
	syncInfo->recentFrames[syncInfo->recentFramesIndex].frame = temp;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_sec = curtime.tv_sec;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_usec = curtime.tv_usec;
	syncInfo->recentFramesIndex++;
	pthread_spin_unlock(&(syncInfo->lock));
	// End Critical Section	

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

	struct timeval curtime;
	gettimeofday(&curtime, NULL);

	struct frameInfo *temp = (struct frameInfo *)malloc(sizeof(struct frameInfo));
	memcpy(temp, frameToSend, sizeof(struct frameInfo));

	// Critical Section
	pthread_spin_lock(&(syncInfo->lock));
	syncInfo->recentFrames[syncInfo->recentFramesIndex].frame = temp;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_sec = curtime.tv_sec;
	syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_usec = curtime.tv_usec;
	syncInfo->recentFramesIndex++;
	pthread_spin_unlock(&(syncInfo->lock));
	// End Critical Section	

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
      pthread_exit(NULL);
    }
    cout << "[DataLink] Received " << iRecvLength << " byte frame from physical." << endl;

#ifdef VERBOSE_RECEIVE_DEBUG
    printf("[DataLink] Received message. Contents: ");
    for(int j = 0; j < iRecvLength; j++) {
      printf("%02X ", pFrame[j]);
    }
    printf("\n");
#endif

    receivedFrame = (struct frameInfo *)malloc(sizeof(struct frameInfo));

    // Disassemble the frame
    if(1 == disassembleFrame(receivedFrame, pFrame, iRecvLength)) {
      cout << "[DataLink] Received invalid frame, ignoring!" << endl;
      free(receivedFrame);
      continue;
    }

    // Check if we received an ACK
    if(receivedFrame->frameType == 1) {
      handleAck(receivedFrame, syncInfo);
      cout << "[DataLink] Processed ACK frame" << endl;
      free(receivedFrame);
      continue;
    } else if(receivedFrame->frameType != 0) {
      // Bad frame type field
      cout << "[DataLink] Received frame with bad Frame Type field!" << endl;
      free(receivedFrame);
      continue;
    }

#ifdef VERBOSE_RECEIVE_DEBUG
    printf("Received message had payload: ");
    for(int p = 0; p < receivedFrame->payloadLength; p++) {
      printf("%02X ", receivedFrame->payload[p]);
    }
    printf("\n");
#endif

    // Check sequence number. See if it's what we were expecting.
    if(receivedFrame->seqNumber != syncInfo->mainSequence) {
      if(receivedFrame->seqNumber >= syncInfo->mainSequence - (WINDOW_SIZE + 1)) {
	cout << "Acking out of order frame with sequence number " << receivedFrame->seqNumber << endl;
	if(sendAck(receivedFrame->seqNumber, syncInfo) != ACK_SIZE) {
	  cout << "[DataLink] Sending ack for sequence number " << receivedFrame->seqNumber << " failed!" << endl;
	}
      } else {
	cout << "[DataLink] Received out of sequence frame (expected sequence number "
	     << syncInfo->mainSequence << ", received " << receivedFrame->seqNumber << ")." << endl;
      }
      free(receivedFrame);
      continue;
    } else {
      // Good sequence number, increment next expected received packet
      // Critical Section
      pthread_spin_lock(&(syncInfo->lock));
      syncInfo->mainSequence++; // Increment expected packet to receive
      syncInfo->ackSequence++; // Increment ACK we're expecting, as we aren't using this sequence number anymore.
      pthread_spin_unlock(&(syncInfo->lock));
      // End Critical Section
    }

    // Block until ack is sent to physical
    if(sendAck(receivedFrame->seqNumber, syncInfo) != ACK_SIZE) {
      cout << "[DataLink] Sending ack for sequence number " << receivedFrame->seqNumber << " failed!" << endl;
      free(receivedFrame);
      continue;
   }

    // Ack has been sent. Extract contents of frame and forward them up to network layer.
    if(receivedFrame->endOfPacket != 1) {
      if(receivedFrame->payloadLength != MAX_PAYLOAD_SIZE) {
	cout << "[DataLink] Frame with invalid size and no end-of-packet marker received!" << endl;
	free(receivedFrame);
	continue; // Bad frame!
      }
      for(int i = 0; i < receivedFrame->payloadLength; i++) {
	stash[i] = receivedFrame->payload[i];
      }
      cout << "[DataLink] Stashing frame to send to DLL" << endl;
      stashReady = 1;
      free(receivedFrame);
      continue;
    } else {
      // Built on the assumption that we receive no more than 2 packets in a row without endOfFrame set.
      if(stashReady == 1) {
	cout << "[DataLink] Unstashing the stash!" << endl;
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
    
#ifdef VERBOSE_IPC_DEBUG
    printf("[DataLink] Sent following packet to network layer: ");
    for(int k = 0; k < iPacketLength; k++) {
      printf("%02X ", pPacket[k]);
    }
    printf("\n");
#endif

    // Block until packet is sent to network
    if ( ( iSendLength = dl_to_nw_send( iSocket, pPacket, iPacketLength ) ) != iPacketLength ) {
      cout << "[DataLink] Error sending packet to network." << endl;
      free(receivedFrame);
      free(pPacket);
      continue;
    }

    free(receivedFrame);
    //free(pPacket);
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
    cout << "[DataLink] Invalid frame passed to transmitFrame! Ignoring!" << endl;
    return 0x00;
  }

  if(frame->payloadLength > MAX_PAYLOAD_SIZE || frame->payloadLength == 0) {
    cout << "[DataLink] Frame with bad payload length received!" << endl;
    return 0x00;
  }

  // Set sequence number for this frame
  frame->seqNumber = syncInfo->mainSequence;
  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->mainSequence++; // Increment sequence number for next sent frame
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section

  // Allocate a buffer to construct the frame in
  buffer = (uint8_t *)malloc(FRAMING_SIZE + frame->payloadLength);

  // We can copy in the first 5 bytes of the frameInfo struct, as they are static fields
  for(i = 0; i < 5; i++) {
    buffer[i] = ((uint8_t *)frame)[i];
  }

  buffer[0] = frame->frameType;
  buffer[1] = (uint8_t)(frame->seqNumber >> 8);
  buffer[2] = (uint8_t)(frame->seqNumber & 0x00FF);
  buffer[3] = frame->endOfPacket;

#ifdef VERBOSE_XMIT_DEBUG
  printf("Transmitting frame with type %X, seqeuence number %X, end of packet %X, payload of length %X\n", 
	 frame->frameType, frame->seqNumber, frame->endOfPacket, frame->payloadLength);
#endif

  // And now we can do the same with the payload
  for(i = 0; i < (frame->payloadLength); i++) {
    buffer[i+(FRAMING_SIZE - 2)] = frame->payload[i];
  }

  // Populate the checksum field
  checksum = generateFCS(buffer, frame->payloadLength + FRAMING_SIZE - 2);
  printf("[DataLink] Calculated FCS as %04X\n", checksum);
  buffer[(FRAMING_SIZE + frame->payloadLength) - 2] = (uint8_t)(checksum >> 8);
  buffer[(FRAMING_SIZE + frame->payloadLength) - 1] = (uint8_t)(checksum & 0x00FF);

  // Wait for window to open up
  while(syncInfo->windowSize == 0);

  // Transmit our frame

#ifdef VERBOSE_XMIT_DEBUG
    printf("Transmitting message. Contents: ");
    for(int j = 0; j < FRAMING_SIZE+frame->payloadLength; j++) {
      printf("%02X ", buffer[j]);
    }
    printf("\n");
#endif

  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->windowSize--; // Decrement available window slots
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, buffer, (FRAMING_SIZE + frame->payloadLength)) ) != (FRAMING_SIZE + frame->payloadLength)) {
    cout << "[DataLink] Error sending frame to physical." << endl;
  }
  cout << "[DataLink] Sent " << toReturn << " byte frame to physical with sequence number " << frame->seqNumber << endl;

  // Now arm timer
  armTimer(frame->seqNumber, syncInfo);

  //free(buffer);

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
  ack[3] = ack[1]; // FCS field matches sequence number
  ack[4] = ack[2];

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, ack, ACK_SIZE) ) != ACK_SIZE) {
    cout << "[DataLink] Error sending ACK to physical." << endl;
  }
  cout << "[DataLink] Sent ACK frame to physical with sequence number " << seqNumber << endl;

  //free(ack);

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
  uint16_t expectedChecksum, receivedChecksum;

  if(frame == 0 || received == 0) {
    return 1;
  } else if(receivedLen < ACK_SIZE || receivedLen > MAX_FRAME_SIZE) {
    cout << "[DataLink] Received frame with bad size (over " << MAX_FRAME_SIZE << " or under " << ACK_SIZE << ")!" << endl;
    return 1;
  }

  frame->frameType = received[0];
  frame->seqNumber = ((uint16_t)(received[1]) << 8) + received[2];

  if(frame->frameType == 0) {
    frame->endOfPacket = received[3];
    frame->payloadLength = receivedLen - FRAMING_SIZE;
  } else {
    frame->endOfPacket = 0;
    frame->payloadLength = 0;
  }

  for(i = (FRAMING_SIZE - 2); i < ((FRAMING_SIZE - 2) + frame->payloadLength); i++) {
    frame->payload[i-(FRAMING_SIZE - 2)] = received[i];
  }

  expectedChecksum = generateFCS(received, (receivedLen - 2));
  receivedChecksum = 0;
  receivedChecksum += received[receivedLen - 2];
  receivedChecksum = receivedChecksum << 8;
  receivedChecksum += received[receivedLen - 1];

  // Verify checksum for data frames
  if(frame->frameType == 0 && expectedChecksum != receivedChecksum) {
    cout << "[DataLink] Received frame with bad FCS (expected " << expectedChecksum << ", received " << receivedChecksum << ")!" << endl;
    return 1;
  } else if(frame->frameType == 1 && (received[1] != received[3] || received[2] != received[4])) { // Same for ACK frames
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
    cout << "[DataLink] Received bad ACK for sequence number " << frame->seqNumber << " (expecting " << syncInfo->ackSequence << ")" << endl;
    return;
  } else {
    cout << "[DataLink] Received ACK for sequence number " << frame->seqNumber << endl;
    // Critical Section
    pthread_spin_lock(&(syncInfo->lock));
    syncInfo->ackSequence++; // Increment next frame we expect an ACK for
    syncInfo->windowSize++; // Increment available window slots

    // We've gotten an ACK for the frame, so remove it from recently transmitted frames; we don't need to resend
    for(int i = 0; i < WINDOW_SIZE + 1; i++) {
      if(syncInfo->recentFrames[i].frame && syncInfo->recentFrames[i].frame->seqNumber == frame->seqNumber) {
	free(syncInfo->recentFrames[i].frame);
	syncInfo->recentFrames[i].frame = NULL;
	break;
      }
    }
    // Alright. Double check if we have any more frames ready. If we do, arm the timer on the first one we find.
    for(int j = syncInfo->recentFramesIndex; j < WINDOW_SIZE + 1; j++) {
      if(syncInfo->recentFrames[j].frame && syncInfo->recentFrames[j].frame->seqNumber > frame->seqNumber) {
	armTimer(syncInfo->recentFrames[j].frame->seqNumber, syncInfo);
	pthread_spin_unlock(&(syncInfo->lock));
	return;
      }
    }
    for(int k = 0; k < syncInfo->recentFramesIndex; k++) {
      if(syncInfo->recentFrames[k].frame && syncInfo->recentFrames[k].frame->seqNumber > frame->seqNumber) {
	armTimer(syncInfo->recentFrames[k].frame->seqNumber, syncInfo);
	pthread_spin_unlock(&(syncInfo->lock));
	return;
      }
    }
    pthread_spin_unlock(&(syncInfo->lock));
    // End Critical Section

    // Pass a known-bad sequence number to disarm the timer otherwise
    armTimer(frame->seqNumber, syncInfo);
  }
}

/**
 * Generate Frame Check Sequence for a frame from its frameInfo struct
 */
uint16_t generateFCS(uint8_t *frame, uint8_t length)
{
  uint16_t checksum = 0x0000;
  int i = 1;

#ifdef VERBOSE_CHECKSUM_DEBUG
  printf("Checksumming message. Contents: ");
  for(int j = 0; j < length; j++) {
    printf("%02X ", frame[j]);
  }
  printf("\n");
#endif

  if(length < 2) {
    return 0;
  }

  checksum += ((uint16_t *)frame)[0];
  length -= 2;

#ifdef VERBOSE_CHECKSUM_DEBUG
  cout << "Checksum start was " << checksum << endl;
#endif

  while(length > 2) {
    checksum ^= ((uint16_t *)frame)[i];
    length -= 2;
    i++;
#ifdef VERBOSE_CHECKSUM_DEBUG
    cout << "Checksum now " << checksum << endl;
#endif
  }

  if(length == 1) {
    checksum ^= frame[length - 1];
#ifdef VERBOSE_CHECKSUM_DEBUG
    cout << "Checksum now " << checksum << endl;
#endif
  }

  return checksum;
}

void handleRetransmission(uint16_t failedFrameSeq, struct linkLayerSync *syncInfo)
{
  int index = WINDOW_SIZE + 2;

  cout << "[DataLink] Retransmitting sequence number " << failedFrameSeq << endl;

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].frame && syncInfo->recentFrames[i].frame->seqNumber == failedFrameSeq) {
      index = i;
      break;
    }
  }

  if(index == WINDOW_SIZE + 2) {
    return;
  }

  // Reset sequence numbers for retransmission
  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->mainSequence = failedFrameSeq;
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section

  // Retransmit the failed frame
  transmitFrame(syncInfo->recentFrames[index].frame, syncInfo);

  // Retransmit any frames after it
  for(int j = syncInfo->recentFramesIndex; j < WINDOW_SIZE + 1; j++) {
    if(syncInfo->recentFrames[j].frame &&
       syncInfo->recentFrames[j].frame->seqNumber > failedFrameSeq) {
      transmitFrame(syncInfo->recentFrames[j].frame, syncInfo);
    }
  }
  for(int k = 0; k < syncInfo->recentFramesIndex; k++) {
    if(syncInfo->recentFrames[k].frame &&
       syncInfo->recentFrames[k].frame->seqNumber > failedFrameSeq) {
      transmitFrame(syncInfo->recentFrames[k].frame, syncInfo);
    }
  }
}

void armTimer(uint16_t seqNumber, struct linkLayerSync *syncInfo)
{
  struct timeval current;
  struct itimerspec value;
  int index = WINDOW_SIZE + 2;

  cout << "[DataLink] Setting timer for sequence number " << seqNumber << endl;

  gettimeofday(&current, NULL);

  pthread_spin_lock(&(syncInfo->lock));

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].frame && syncInfo->recentFrames[i].frame->seqNumber == seqNumber) {
      index = i;
      break;
    }
  }

  if(index == WINDOW_SIZE + 2) {
    // Couldn't find the frame in question. Disarm the timer.
    value.it_value.tv_sec = 0;
    value.it_value.tv_nsec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;
    timer_settime(syncInfo->timer, 0, &value, NULL);
    pthread_spin_unlock(&(syncInfo->lock));
    cout << "[DataLink] Disarmed timer" << endl;
    return;
  }

  // Seconds counter ticked.
  if(syncInfo->recentFrames[index].transmitTime.tv_sec != current.tv_sec) {
    current.tv_usec += 1000000; // Add one second's worth of microseconds to the current microsecond count
  }

  // Timeout already expired. Retransmit the frame.
  if(current.tv_usec - syncInfo->recentFrames[index].transmitTime.tv_usec >= TIMEOUT_US) {
    pthread_spin_unlock(&(syncInfo->lock));
    value.it_value.tv_sec = 0;
    value.it_value.tv_nsec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;
    timer_settime(syncInfo->timer, 0, &value, NULL);
    cout << "[DataLink] Disarmed timer" << endl;
    cout << "[DataLink] Frame timed out, retransmitting!" << endl;
    handleRetransmission(seqNumber, syncInfo);
    return;
  }

  // Alright. Disarm the timer, then rearm with new time.
  value.it_value.tv_sec = 0;
  value.it_value.tv_nsec = ((TIMEOUT_US) - (current.tv_usec - syncInfo->recentFrames[index].transmitTime.tv_usec)) * 1000;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_nsec = 0;

  pthread_spin_unlock(&(syncInfo->lock));

  cout << "[DataLink] Armed timer with time of " << value.it_value.tv_nsec << " seconds for frame " 
       << syncInfo->recentFrames[index].frame->seqNumber << endl;

  timer_settime(syncInfo->timer, 0, &value, NULL);
}
