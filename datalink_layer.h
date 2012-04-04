#include <stdint.h>
#include <pthread.h>

#ifndef DATALINK_LAYER_H
#define DATALINK_LAYER_H

void * DataLinkLayer( void * longPointer );

// Size of the sliding window. 1 for Go Back 1, 4 for Go Back 4.
#define WINDOW_SIZE 1
// Maximum size of an acceptable payload, in bytes
#define MAX_PAYLOAD_SIZE 143
// Maximum size of an acceptable frame, in bytes
#define MAX_FRAME_SIZE 150
// Size of an ACK frame, in bytes
#define ACK_SIZE 7

// Structure of an ACK frame
struct ackFrameInfo {
  uint8_t frameType;
  uint16_t seqNumber;
  uint16_t ignored; // Represents end of packet and length fields, ignored in an ACK
  uint16_t seqNumberRepeat;
};

// Frame info structure: Contains all fields in a completed frame, in order.
struct frameInfo {
  uint8_t frameType;
  uint16_t seqNumber;
  uint8_t endOfPacket;
  uint8_t payloadLength;
  uint8_t payload[MAX_PAYLOAD_SIZE];
  uint16_t frameCheckSequence;
};
  
// Syncs the send and receive threads for a single physical layer socket
struct linkLayerSync {
  int socket;
  uint8_t windowSize;
  uint16_t sendSequence;
  uint16_t ackSequence;
  uint16_t recvSequence;
  pthread_spinlock_t lock; // Used for sync between send and receive threads
};

#endif
