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
#include "global.h"

using namespace std;

// prototypes
void * NwToPhHandler( void * longPointer );
void * PhToNwHandler( void * longPointer );
uint16_t generateFCS(uint8_t *frame, uint8_t length);
void handleRetransmission(uint16_t failedFrameSeq, struct linkLayerSync *syncInfo);
void sigHandler(int sig, siginfo_t *si, void *uc);

// Globals.
struct linkLayerSync *syncOne, *syncTwo, *syncThree;

void * DataLinkLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  pthread_t iNwToPhThreadId; // physical layer receive thread id
  pthread_t iPhToNwThreadId; // physical layer send thread id
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)malloc(sizeof(struct linkLayerSync));
  struct sigaction sa;
  struct sigevent sev;
  static int signalAvail[3] = {1, 1, 1};
  int sigNo;

  // Populate signal handling structures
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_value.sival_ptr = (void *)(&syncInfo);

  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = sigHandler;

  if(signalAvail[0] == 1) {
    signalAvail[0] = 0;
    sigNo = 0;
    sev.sigev_signo = SIGUSR1;
    if( -1 == sigaction(SIGUSR1, &sa, NULL)) {
      cout << "Error configuring signals!" << endl;
      return (void *)NULL;
    }
    syncOne = syncInfo;    
  } else if(signalAvail[1] == 1) {
    signalAvail[1] = 0;
    sigNo = 1;
    sev.sigev_signo = SIGUSR2;
    if( -1 == sigaction(SIGUSR2, &sa, NULL)) {
      cout << "Error configuring signals!" << endl;
      return (void *)NULL;
    }
    syncTwo = syncInfo;    
  } else {
    signalAvail[2] = 0;
    sigNo = 2;
    sev.sigev_signo = SIGALRM;
    if( -1 == sigaction(SIGALRM, &sa, NULL)) {
      cout << "Error configuring signals!" << endl;
      return (void *)NULL;
    }
    syncThree = syncInfo;    
  }

  // Populate the syncronization structure assigned to the threads
  syncInfo->socket = iSocket;
  syncInfo->windowSize = WINDOW_SIZE;
  syncInfo->mainSequence = 0;
  syncInfo->ackSequence = 0;
  syncInfo->recentFramesIndex = 0;
  syncInfo->acksUntilBad = 0;
  syncInfo->dataUntilBad = 0;
  syncInfo->timerRunning = 0;
  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    syncInfo->recentFrames[i].isValid = 0;
  }
  // Initialize the spinlock for syncronization
  pthread_spin_init(&(syncInfo->lock), 0);

  // Create a timer for timeouts
  timer_create(CLOCK_REALTIME, &sev, &(syncInfo->timer));
  
  cout << "[DataLink] Initializing..." << endl;
  
  // Create two threads for send and receiving
  pthread_create(&iNwToPhThreadId, NULL, &NwToPhHandler, (void *) syncInfo);
  pthread_create(&iPhToNwThreadId, NULL, &PhToNwHandler, (void *) syncInfo);
  pthread_join(iNwToPhThreadId, NULL);
  pthread_join(iPhToNwThreadId, NULL);
  
  // Free the sync structure
  //cout << "Freeing, line 86" << endl;
  //free(syncInfo);

  signalAvail[sigNo] = 1;

  cout << "[DataLink] Terminating." << endl;
  pthread_exit(NULL);
}

void * NwToPhHandler( void * longPointer )
{

  intptr_t iSocket = (intptr_t) longPointer; // client socket handle
  char *pFrame;
  int iFrameLength;
  int iSendLength; // Length of send data
  int iRecvLength; // length of recieved data
  struct frameInfo *frameToSend;
  struct linkLayerSync *syncInfo = (struct linkLayerSync *)longPointer;

  iSocket = syncInfo->socket;

  while ( true )
    {
      char * pPacket; // packet pointer

      int toOr;
      while(1) {
	toOr = 0;
	for(int h = 0; h < WINDOW_SIZE + 1; h++) {
	  toOr = toOr | syncInfo->recentFrames[h].isValid;
	}
	if(toOr == 0)
	  break;
      }
    
      // Block until packet is received from network
      if ( ( iRecvLength = nw_to_dl_recv( iSocket, &pPacket ) ) == -1 ) {
	cout << "[DataLink] Error receiving packet from network." << endl;
	pthread_exit(NULL);
      }
      if (g_debug) cout << "[DataLink] Received " << iRecvLength << " byte packet from network." << endl;
    
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

	if(!sendData(frameToSend, syncInfo) == frameToSend->payloadLength + FRAMING_SIZE) {
	  cout << "[DataLink] Transmission failed!";
	}

#ifdef VERBOSE_XMIT_DEBUG
	cout << "[DataLink] Sending second frame in series!" << endl;
#endif

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 125000000;
	nanosleep(&ts, NULL);

	frameToSend->frameType = 0x00;
	frameToSend->endOfPacket = 0x01;
	
	// Copy in payload
	for(int i = 0; i < iRecvLength - MAX_PAYLOAD_SIZE; i++) {
	  frameToSend->payload[i] = pPacket[i + MAX_PAYLOAD_SIZE];
	}

	if(!sendData(frameToSend, syncInfo) == frameToSend->payloadLength + FRAMING_SIZE) {
	  cout << "[DataLink] Transmission failed!";
	}

	free(frameToSend);
      } else {
	frameToSend = (struct frameInfo *)malloc(sizeof(struct frameInfo));

	frameToSend->frameType = 0x00;
	frameToSend->endOfPacket = 0x01;
	frameToSend->payloadLength = iRecvLength;
	frameToSend->seqNumber = syncInfo->mainSequence;
      
	// Copy in the payload
	for(int i = 0; i < iRecvLength; i++) {
	  frameToSend->payload[i] = pPacket[i];
	}

	if(!sendData(frameToSend, syncInfo) == frameToSend->payloadLength + FRAMING_SIZE) {
	  cout << "[DataLink] Transmission failed!";
	}

	free(frameToSend);
      }
    }
}

void * PhToNwHandler( void * longPointer )
{
  intptr_t iSocket; // client socket handle
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
    if (g_debug) cout << "[DataLink] Received " << iRecvLength << " byte frame from physical." << endl;

#ifdef VERBOSE_RECEIVE_DEBUG
    printf("[DataLink] Received message. Contents: ");
    for(int j = 0; j < iRecvLength; j++) {
      printf("%02X ", pFrame[j]);
    }
    printf("\n");
#endif

    receivedFrame = (struct frameInfo *)malloc(sizeof(struct frameInfo));

    // Disassemble the frame
    if(1 == disassembleFrame(iRecvLength, pFrame, receivedFrame, syncInfo)) {
      cout << "[DataLink] Received invalid frame, ignoring!" << endl;
      free(receivedFrame);
      continue;
    }

    // Check if we received an ACK
    if(receivedFrame->frameType == 1) {
      processAck(receivedFrame->seqNumber, syncInfo);
#ifdef VERBOSE_RECEIVE_DEBUG 
      cout << "[DataLink] Processed ACK frame" << endl;
#endif
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
      //cout << "Freeing, line 319" << endl;
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
      //cout << "Freeing, line 335" << endl;
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
#ifdef VERBOSE_RECEIVE_DEBUG
      cout << "[DataLink] Stashing frame to send to DLL" << endl;
#endif
      stashReady = 1;
      //cout << "Freeing, line 353" << endl;
      free(receivedFrame);
      continue;
    } else {
      // Built on the assumption that we receive no more than 2 packets in a row without endOfFrame set.
      if(stashReady == 1) {
#ifdef VERBOSE_RECEIVE_DEBUG
	cout << "[DataLink] Unstashing the stash!" << endl;
#endif
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
	  pthread_exit(NULL);
    }

    free(receivedFrame);
    //free(pPacket);
    if (g_debug) cout << "[DataLink] Sent " << iSendLength << " byte packet to network." << endl;
  }
}

#if 0
// OLD REFERENCE CODE. NOT ACTUALLY COMPILED.

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
    int id = WINDOW_SIZE + 2;
    for(int i = 0; i < WINDOW_SIZE + 1; i++) {
#ifdef VERBOSE_XMIT_DEBUG
      cout << "[DataLink] Testing entry " << i << " - valid bit is " << syncInfo->recentFrames[i].isValid << endl;
      if(syncInfo->recentFrames[i].isValid) {
	cout << "[DataLink] Entry " << i << " has sequence number " << syncInfo->recentFrames[i].frame->seqNumber << endl;
      }
#endif
      if(syncInfo->recentFrames[i].isValid && syncInfo->recentFrames[i].frame->seqNumber == frame->seqNumber) {
	id = i;
	break;
      }
    }
    if(id == WINDOW_SIZE + 2) {
      pthread_spin_unlock(&(syncInfo->lock));
      cout << "[DataLink] Could not find recent frames entry to free!" << endl;
      return;
    }
    free(syncInfo->recentFrames[id].frame);
    syncInfo->recentFrames[id].isValid = 0;
    // Alright. Double check if we have any more frames ready. If we do, arm the timer on the first one we find.
    for(int j = syncInfo->recentFramesIndex; j < WINDOW_SIZE + 1; j++) {
      if(syncInfo->recentFrames[j].isValid && syncInfo->recentFrames[j].frame->seqNumber > frame->seqNumber) {
	pthread_spin_unlock(&(syncInfo->lock));
	armTimer(syncInfo->recentFrames[j].frame->seqNumber, syncInfo);
	return;
      }
    }
    for(int k = 0; k < syncInfo->recentFramesIndex; k++) {
      if(syncInfo->recentFrames[k].isValid && syncInfo->recentFrames[k].frame->seqNumber > frame->seqNumber) {
	pthread_spin_unlock(&(syncInfo->lock));
	armTimer(syncInfo->recentFrames[k].frame->seqNumber, syncInfo);
	return;
      }
    }
    pthread_spin_unlock(&(syncInfo->lock));
    // End Critical Section

    // Pass a known-bad sequence number to disarm the timer otherwise
    armTimer(frame->seqNumber, syncInfo);
  }
}
#endif

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
  struct timeval curtime;
  
  cout << "[DataLink] Retransmitting sequence number " << failedFrameSeq << endl;

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].isValid && syncInfo->recentFrames[i].frame->seqNumber == failedFrameSeq) {
      index = i;
      break;
    }
  }

  if(index == WINDOW_SIZE + 2) {
    return;
  }

  cout << "[DataLink] Retransmitting frame with sequence number " << failedFrameSeq << " now" << endl;

  gettimeofday(&curtime, NULL);

  // Reset frame transmit time
  syncInfo->recentFrames[i].transmitTime.tv_sec = curtime.tv_sec;
  syncInfo->recentFrames[i].transmitTime.tv_usec = curtime.tv_usec;

  // Retransmit the failed frame
  sendToPhysical(syncInfo->recentFrames[index].frame, syncInfo);

  // Rearm the timer for the frame
  armTimer(failedFrameSeq, syncInfo);
}

void sigHandler(int sig, siginfo_t *si, void *uc)
{
  struct linkLayerSync *syncInfo;
  int lowestSequence = 0;

  cout << "[DataLink] frame timed out" << endl;

  if(sig == SIGUSR1) {
    syncInfo = syncOne;
  } else if(sig == SIGUSR2) {
    syncInfo = syncTwo;
  } else {
    syncInfo = syncThree;
  }

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].isValid) {
      lowestSequence = syncInfo->recentFrames[i].frame->seqNumber;
      break;
    }
  }

  for(int i = 0; i < WINDOW_SIZE + 1; i++) {
    if(syncInfo->recentFrames[i].isValid && syncInfo->recentFrames[i].frame->seqNumber < lowestSequence) {
      lowestSequence = syncInfo->recentFrames[i].frame->seqNumber;
    }
  }

  handleRetransmission(lowestSequence ,syncInfo);
}
