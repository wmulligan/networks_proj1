#include <stdint.h>
#include <pthread.h>

#ifndef DATALINK_LAYER_H
#define DATALINK_LAYER_H

void * DataLinkLayer( void * longPointer );

// Size of the sliding window. 1 for Go Back 1, 4 for Go Back 4.
#define WINDOW_SIZE 1
// Maximum size of an acceptable payload, in bytes
#define MAX_PAYLOAD_SIZE 150
// Maximum size of an acceptable frame, in bytes
#define MAX_FRAME_SIZE 156
// Size of the framing bytes
#define FRAMING_SIZE 6
// Size of an ACK frame, in bytes
#define ACK_SIZE 5
// Timeout in microseconds
#define TIMEOUT_US 100000
// Number of ACKs before a bad ACK. This is actually n+1 due to the way I coded it.
#define BAD_ACKS 7
// Number of data frames before a bad data frame. Again n+1.
#define BAD_DATA 5

// This is really verbose debug on checksum generation.
#ifdef VERBOSE_CHECKSUM_DEBUG
#undefine VERBOSE_CHECKSUM_DEBUG
#endif
// Verbose debug on receive
#ifdef VERBOSE_RECEIVE_DEBUG
#undefine VERBOSE_RECEIVE_DEBUG
#endif
// Verbose debug on transmit
#ifdef VERBOSE_XMIT_DEBUG
#undefine VERBOSE_XMIT_DEBUG
#endif
// Verbose interlayer communication debug
#ifdef VERBOSE_IPC_DEBUG
#undefine VERBOSE_IPC_DEBUG
#endif

// Frame info structure: Contains all fields in a completed frame, in order.
struct frameInfo {
  uint8_t frameType;
  uint16_t seqNumber;
  uint8_t endOfPacket;
  uint8_t payloadLength;
  uint8_t payload[MAX_PAYLOAD_SIZE];
};

// Holds frames transmitted by datalink layer
struct transmittedFrame {
  struct timeval transmitTime;
  struct frameInfo *frame;
  int isValid;
};
  
// Syncs the send and receive threads for a single physical layer socket
struct linkLayerSync {
  timer_t timer;
  int socket;
  uint8_t windowSize;
  uint16_t mainSequence;
  uint16_t ackSequence;
  uint8_t acksUntilBad;
  uint8_t dataUntilBad;
  struct transmittedFrame recentFrames[WINDOW_SIZE + 1];
  uint8_t recentFramesIndex;
  pthread_spinlock_t lock; // Used for sync between send and receive threads
};

#endif
