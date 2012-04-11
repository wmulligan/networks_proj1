/**
 * Contains datalink layer helpers
 *
 * @file datalink_layer.cpp
 * @author Matthew Heon
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
#include "global.h"

using namespace std;

uint16_t generateFCS(uint8_t *frame, uint8_t length);

/**
 * Checks fields of a data frame, sans sequence number, for validity
 */
uint8_t isFrameValid(struct frameInfo *frame)
{
  if(!frame) {
    cout << "[DataLink] Null passed as an argument!" << endl;
  }

  if(frame->frameType != 0) {
    return 0;
  }

  if(frame->endOfPacket > 1) {
    return 0;
  }

  if(frame->payloadLength == 0) {
    return 0;
  }

  return 1;
}

/**
 * Just constructs a frame and sends it to physical layer. No checks for validity.
 */
uint8_t sendToPhysical(struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  int toReturn = 0;
  uint16_t checksum;

  if(frame == NULL || syncInfo == NULL) {
    cout << "[DataLink] Null passed as argument!" << endl;
  }

  uint8_t *buffer = (uint8_t *)malloc(FRAMING_SIZE + frame->payloadLength);

  buffer[0] = frame->frameType;
  buffer[1] = (uint8_t)(frame->seqNumber >> 8);
  buffer[2] = (uint8_t)(frame->seqNumber & 0x00FF);
  buffer[3] = frame->endOfPacket;

  // And now we can do the same with the payload
  for(int i = 0; i < (frame->payloadLength); i++) {
    buffer[i+(FRAMING_SIZE - 2)] = frame->payload[i];
  }

  // Populate the checksum field
  checksum = generateFCS(buffer, frame->payloadLength + FRAMING_SIZE - 2);
#ifdef VERBOSE_XMIT_DEBUG
  printf("[DataLink] Calculated FCS as %04X\n", checksum);
#endif
  buffer[(FRAMING_SIZE + frame->payloadLength) - 2] = (uint8_t)(checksum >> 8);
  buffer[(FRAMING_SIZE + frame->payloadLength) - 1] = (uint8_t)(checksum & 0x00FF);

#ifdef GENERATE_ERRORS
  if(syncInfo->dataUntilBad == BAD_DATA) {
    cout << "[DataLink] Altering checksum for bad frame!" << endl;
    buffer[(FRAMING_SIZE + frame->payloadLength) - 1] += 1; // Bad checksum
    pthread_spin_lock(&(syncInfo->lock));
    syncInfo->totalBad++;
    pthread_spin_unlock(&(syncInfo->lock));    
  }

  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->dataUntilBad++;
  if(syncInfo->dataUntilBad > BAD_DATA) {
    syncInfo->dataUntilBad = 0;
  }
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section
#endif

#ifdef VERBOSE_XMIT_DEBUG
  printf("Transmitting message. Contents: ");
  for(int j = 0; j < FRAMING_SIZE+frame->payloadLength; j++) {
    printf("%02X ", buffer[j]);
  }
  printf("\n");
#endif
  
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->totalFrames++;
  pthread_spin_unlock(&(syncInfo->lock));    

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, buffer, (FRAMING_SIZE + frame->payloadLength)) ) != (FRAMING_SIZE + frame->payloadLength)) {
    cout << "[DataLink] Error sending frame to physical." << endl;
  }
  if (g_debug) cout << "[DataLink] Sent " << toReturn << " byte frame to physical with sequence number " << frame->seqNumber << endl;

  return toReturn;
}

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

  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->totalAcks++;
  pthread_spin_unlock(&(syncInfo->lock));    

#ifdef GENERATE_ERRORS
  if(syncInfo->acksUntilBad == BAD_ACKS) {
    cout << "[DataLink] Altering checksum for bad frame!" << endl;
    ack[4] += 1;
    pthread_spin_lock(&(syncInfo->lock));
    syncInfo->totalBad++;
    pthread_spin_unlock(&(syncInfo->lock));    
  }

  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->acksUntilBad++;
  if(syncInfo->acksUntilBad > BAD_ACKS) {
    syncInfo->acksUntilBad = 0;
  }
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section
#endif

  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, ack, ACK_SIZE) ) != ACK_SIZE) {
    cout << "[DataLink] Error sending ACK to physical." << endl;
	pthread_exit(NULL);
  }
  if (g_debug) cout << "[DataLink] Sent ACK frame to physical with sequence number " << seqNumber << endl;

  return toReturn;
}

/**
 * Send a new data frame. We expect that everything in the frameInfo struct except sequence number is good.
 */
uint8_t sendData(struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  uint8_t toReturn = 0;
  uint16_t seqNum;

  if(!syncInfo) {
    return toReturn;
  }

  // Check if frame is valid
  if(!isFrameValid(frame)) {
    return toReturn;
  }

  // Spin until our window is open
  while(syncInfo->windowSize == 0);

  // Decrement window size
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->windowSize--;
  pthread_spin_unlock(&(syncInfo->lock));  

  // Set sequence number
  seqNum = getNewSequence(syncInfo);
  frame->seqNumber = seqNum;
  cout << "[DataLink] Got sequence number " << seqNum << " for frame" << endl;

  // Transmit the frame
  sendToPhysical(frame, syncInfo);

  // Store information on frame for transmission attempts
  storeFrameInfo(frame, syncInfo);

  // Check if timer is running
  if(!syncInfo->timerRunning) {
    armTimer(seqNum, syncInfo);
  }

  return toReturn;
}

/**
 * Get a new data frame sequence number
 */
uint16_t getNewSequence(struct linkLayerSync *syncInfo)
{
  uint16_t sequenceNumber = 0;

  if(!syncInfo) {
    return 0;
  }

  // Get a sequence number
  pthread_spin_lock(&(syncInfo->lock));
  sequenceNumber = syncInfo->mainSequence;
  syncInfo->mainSequence++;
  pthread_spin_unlock(&(syncInfo->lock));

  return sequenceNumber;
}

/**
 * Process an ACK frame.
 */
void processAck(uint16_t seqNum, struct linkLayerSync *syncInfo) 
{
  if(!syncInfo) {
    return;
  }

  if(seqNum != syncInfo->ackSequence) {
    cout << "[DataLink] Got out of sequence ack (got " << seqNum << ", expecting " << syncInfo->ackSequence << ")" 
	 << endl;

    if(seqNum < syncInfo->ackSequence) {
      clearFrameInfo(seqNum, syncInfo);
      
      // Disarm the timer
      disarmTimer(syncInfo);
      // Arm with next frame
      if(seqNum + 1 < syncInfo->mainSequence)
	armTimer(seqNum + 1, syncInfo);      

      // Increase our window size
      pthread_spin_lock(&(syncInfo->lock));
      if(syncInfo->windowSize <= WINDOW_SIZE)
	syncInfo->windowSize++;
      pthread_spin_unlock(&(syncInfo->lock));  
    }
   
    return;
  }

  cout << "[DataLink] Got ACK for sequence number " << seqNum << endl;

  clearFrameInfo(seqNum, syncInfo);

  // Disarm the timer
  disarmTimer(syncInfo);
  // Arm with next frame
  if(seqNum + 1 < syncInfo->mainSequence)
    armTimer(seqNum + 1, syncInfo);

  // Increase our window size
  pthread_spin_lock(&(syncInfo->lock));
  if(syncInfo->windowSize <= WINDOW_SIZE)  
    syncInfo->windowSize++;
  syncInfo->ackSequence++;
  pthread_spin_unlock(&(syncInfo->lock));  
}

/**
 * Store frame information for retransmission
 */
void storeFrameInfo(struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  struct timeval curtime;
  struct frameInfo *temp = (struct frameInfo *)malloc(sizeof(struct frameInfo));

  memcpy(temp, frame, sizeof(struct frameInfo));
  gettimeofday(&curtime, NULL);
    
#ifdef VERBOSE_XMIT_DEBUG
  cout << "[DataLink] Storing recent frame with sequence number " << temp->seqNumber << endl;
#endif
  
  // Critical Section
  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->recentFrames[syncInfo->recentFramesIndex].frame = temp;
  syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_sec = curtime.tv_sec;
  syncInfo->recentFrames[syncInfo->recentFramesIndex].transmitTime.tv_usec = curtime.tv_usec;
  syncInfo->recentFrames[syncInfo->recentFramesIndex].isValid = 1;
  syncInfo->recentFramesIndex++;
  if(syncInfo->recentFramesIndex >= WINDOW_SIZE + 1) {
    syncInfo->recentFramesIndex = 0;
  }
  pthread_spin_unlock(&(syncInfo->lock));
  // End Critical Section	
}

/**
 * Clear a frame's information from the retransmission struct.
 */
void clearFrameInfo(uint16_t seqNum, struct linkLayerSync *syncInfo)
{
  int index = WINDOW_SIZE + 2;

  if(!syncInfo) {
    cout << "[Datalink] Null pointer passed!" << endl;
    return;
  }

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].isValid && syncInfo->recentFrames[i].frame->seqNumber == seqNum) {
      index = i;
      break;
    }
  }

  if(index == WINDOW_SIZE + 2) {
    return;
  }

  cout << "[DataLink] Freeing frame info structure" << endl;

  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->recentFrames[index].isValid = 0;
  free(syncInfo->recentFrames[index].frame);
  pthread_spin_unlock(&(syncInfo->lock));
}

/**
 * Disassemble a frame into a frameInfo struct
 */
uint8_t disassembleFrame(uint8_t length, uint8_t *bareFrame, struct frameInfo *frame, struct linkLayerSync *syncInfo)
{
  uint8_t toReturn = 0;
  uint16_t expectedChecksum, receivedChecksum;

  if(length == 0 || !bareFrame || !frame || !syncInfo) {
    return 1;
  }

  // Got an ACK
  if(bareFrame[0] == 1) {
    if(length != ACK_SIZE) {
      cout << "[DataLink] Got bad-sized ack!" << endl;
      return 1;
    }
    if(bareFrame[1] != bareFrame[3] || bareFrame[2] != bareFrame[4]) {
      cout << "[DataLink] Got Ack with bad sequence numbers (not matching)" << endl;
      return 1;
    }

    frame->frameType = 1;
    frame->seqNumber = ((uint16_t)(bareFrame[1]) << 8) + bareFrame[2];

    cout << "[DataLink] Received ACK with sequence number " << frame->seqNumber << endl;

    return 0;
  }

  // Did not get an ACK.
  if(bareFrame[0] != 0) {
    cout << "[DataLink] Got frame with bad type field!" << endl;
    return 1;
  }

  // Check checksum before we go further
  expectedChecksum = generateFCS(bareFrame, (length - 2));
  receivedChecksum = 0;
  receivedChecksum += bareFrame[length - 2];
  receivedChecksum = receivedChecksum << 8;
  receivedChecksum += bareFrame[length - 1];

  if(expectedChecksum != receivedChecksum) {
    cout << "[DataLink] Received frame with bad checksum" << endl;
    return 1;
  }

  frame->frameType = 0;
  frame->seqNumber = ((uint16_t)(bareFrame[1]) << 8) + bareFrame[2];
  frame->endOfPacket = bareFrame[3];

  if(frame->endOfPacket > 1) {
    cout << "[DataLink] Got frame with bad End of Packet flag!" << endl;
    return 1;
  }

  frame->payloadLength = length - FRAMING_SIZE;

  for(int i = (FRAMING_SIZE - 2); i < ((FRAMING_SIZE - 2) + frame->payloadLength); i++) {
    frame->payload[i-(FRAMING_SIZE - 2)] = bareFrame[i];
  }
  free(bareFrame);

  return toReturn;
}

void disarmTimer(struct linkLayerSync *syncInfo)
{
  struct itimerspec value;

  if(!syncInfo) {
    return;
  }
  
  value.it_value.tv_sec = 0;
  value.it_value.tv_nsec = 0;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_nsec = 0;
  timer_settime(syncInfo->timer, 0, &value, NULL);

  // Timer was disarmed, mark it as such
  pthread_spin_lock(&(syncInfo->lock));  
  syncInfo->timerRunning = 0;
  pthread_spin_unlock(&(syncInfo->lock));
  
  cout << "[DataLink] Disarmed timer" << endl;
}

void armTimer(uint16_t seqNum, struct linkLayerSync *syncInfo)
{
  struct itimerspec value;
  struct timeval current;
  int index = WINDOW_SIZE + 2;

  gettimeofday(&current, NULL);

  if(!syncInfo) {
    return;
  }

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].isValid && syncInfo->recentFrames[i].frame->seqNumber == seqNum) {
      index = i;
      break;
    }
  }

  if(index == WINDOW_SIZE + 2) {
    cout << "[DataLink] Could not arm timer for sequence number "<< seqNum << endl;
    return;
  }

  if(syncInfo->timerRunning) {
    disarmTimer(syncInfo);
  }

    // Seconds counter ticked.
  if(syncInfo->recentFrames[index].transmitTime.tv_sec != current.tv_sec) {
    current.tv_usec += 1000000; // Add one second's worth of microseconds to the current microsecond count
  }

  // Timeout already expired. Retransmit the frame.
  if(current.tv_usec - syncInfo->recentFrames[index].transmitTime.tv_usec >= TIMEOUT_US) {
    disarmTimer(syncInfo);
    
    return;
  }

  // Alright. Disarm the timer, then rearm with new time.
  value.it_value.tv_sec = 0;
  value.it_value.tv_nsec = ((TIMEOUT_US) - (current.tv_usec - syncInfo->recentFrames[index].transmitTime.tv_usec)) * 1000;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_nsec = 0;

  cout << "[DataLink] Armed timer with time of " << value.it_value.tv_nsec << " nseconds for frame " 
       << syncInfo->recentFrames[index].frame->seqNumber << endl;

  pthread_spin_lock(&(syncInfo->lock));
  syncInfo->timerRunning = 1;
  pthread_spin_unlock(&(syncInfo->lock));

  timer_settime(syncInfo->timer, 0, &value, NULL);
}
