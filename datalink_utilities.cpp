/**
 * Contains datalink layer functionality of protocol
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

/**
 * Checks fields of a data frame, sans sequence number, for validity
 */
uint8_t isFrameValid(struct frameInfo *frame)
{
  if(!frame) {
    cout << "[DataLink] Null passed as an argument!" << endl;
  }

  if(frame->frameType != 0) {
    return 1;
  }

  if(frame->endOfPacket > 1) {
    return 1;
  }

  if(frame->payloadLength == 0) {
    return 1;
  }

  return 0;
}

/**
 * Just constructs a frame and sends it to physical layer. No checks for validity.
 */
uint8_t sendToPhysical(struct frameInfo *frame, struct linkLayerSync *sync)
{
  int toReturn = 0;

  if(!frame || !sync) {
    cout << "[DataLink] Null passed as argument!" << endl;
  }

  uint8_t *buffer = (uint8_t *)malloc(FRAMING_SIZE + frame->payloadLength);

  buffer[0] = frame->frameType;
  buffer[1] = (uint8_t)(frame->seqNumber >> 8);
  buffer[2] = (uint8_t)(frame->seqNumber & 0x00FF);
  buffer[3] = frame->endOfPacket;

  // And now we can do the same with the payload
  for(i = 0; i < (frame->payloadLength); i++) {
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
  }
#endif

#ifdef VERBOSE_XMIT_DEBUG
  printf("Transmitting message. Contents: ");
  for(int j = 0; j < FRAMING_SIZE+frame->payloadLength; j++) {
    printf("%02X ", buffer[j]);
  }
  printf("\n");
#endif
  
  // Block until frame is sent to physical
  if((toReturn = dl_to_ph_send(syncInfo->socket, buffer, (FRAMING_SIZE + frame->payloadLength)) ) != (FRAMING_SIZE + frame->payloadLength)) {
    cout << "[DataLink] Error sending frame to physical." << endl;
  }
  if (g_debug) cout << "[DataLink] Sent " << toReturn << " byte frame to physical with sequence number " << frame->seqNumber << endl;

  return toReturn;
}
