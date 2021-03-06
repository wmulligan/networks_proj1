/* queue.cpp
 * Contains functions to send data between layers
 *
 * Author(s):
 * Will Mulligan
 */
#include <iostream>
#include <map>
#include <queue>
#include <semaphore.h>

#include "queue.h"

using namespace std;

struct sBuffer {
  sBuffer(char * pBuffer, int iLength) : pBuffer(pBuffer), iLength(iLength) { }
  char * pBuffer;
  int iLength;
};

map< int, queue< sBuffer* > > ph_to_dl;
map< int, queue< sBuffer* > > dl_to_ph;
map< int, queue< sBuffer* > > dl_to_nw;
map< int, queue< sBuffer* > > nw_to_dl;
map< int, queue< sBuffer* > > nw_to_ap;
map< int, queue< sBuffer* > > ap_to_nw;

map< int, sem_t > sem_ph_to_dl;
map< int, sem_t > sem_dl_to_ph;
map< int, sem_t > sem_dl_to_nw;
map< int, sem_t > sem_nw_to_dl;
map< int, sem_t > sem_nw_to_ap;
map< int, sem_t > sem_ap_to_nw;

void initQueue( int id )
{
  sem_init(&sem_ph_to_dl[id], 0, 0);
  sem_init(&sem_dl_to_ph[id], 0, 0);
  sem_init(&sem_dl_to_nw[id], 0, 0);
  sem_init(&sem_nw_to_dl[id], 0, 0);
  sem_init(&sem_nw_to_ap[id], 0, 0);
  sem_init(&sem_ap_to_nw[id], 0, 0);
}

void terminateQueue( int id )
{
  sem_post(&sem_ph_to_dl[id]);
  sem_post(&sem_dl_to_ph[id]);
  sem_post(&sem_dl_to_nw[id]);
  sem_post(&sem_nw_to_dl[id]);
  sem_post(&sem_nw_to_ap[id]);
  sem_post(&sem_ap_to_nw[id]);
  sem_destroy(&sem_ph_to_dl[id]);
  sem_destroy(&sem_dl_to_ph[id]);
  sem_destroy(&sem_dl_to_nw[id]);
  sem_destroy(&sem_nw_to_dl[id]);
  sem_destroy(&sem_nw_to_ap[id]);
  sem_destroy(&sem_ap_to_nw[id]);
}

// Physical to DataLink
int ph_to_dl_send( int id, char * pFrame, int iFrameLength ) {
  ph_to_dl[id].push(new sBuffer(pFrame, iFrameLength));
  sem_post(&sem_ph_to_dl[id]);
  return iFrameLength;
}

int ph_to_dl_recv( int id, char ** pFrame ) {
  sem_wait(&sem_ph_to_dl[id]);
  if ( ! ph_to_dl[id].empty() ) {
    sBuffer *buffer = ph_to_dl[id].front();
    ph_to_dl[id].pop();
    *pFrame = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

// DataLink to Physical
int dl_to_ph_send( int id, char * pFrame, int iFrameLength ) {
  dl_to_ph[id].push(new sBuffer(pFrame, iFrameLength));
  sem_post(&sem_dl_to_ph[id]);
  return iFrameLength;
}

int dl_to_ph_recv( int id, char ** pFrame ) {
  sem_wait(&sem_dl_to_ph[id]);
  if ( ! dl_to_ph[id].empty() ) {
    sBuffer *buffer = dl_to_ph[id].front();
    dl_to_ph[id].pop();
    *pFrame = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

// DataLink to Network
int dl_to_nw_send( int id, char * pPacket, int iPacketLength ) {
  dl_to_nw[id].push(new sBuffer(pPacket, iPacketLength));
  sem_post(&sem_dl_to_nw[id]);
  return iPacketLength;
}
int dl_to_nw_recv( int id, char ** pPacket ) {
  sem_wait(&sem_dl_to_nw[id]);
  if ( ! dl_to_nw[id].empty() ) {
    sBuffer *buffer = dl_to_nw[id].front();
    dl_to_nw[id].pop();
    *pPacket = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

// Network to DataLink
int nw_to_dl_send( int id, char * pPacket, int iPacketLength ) {
  nw_to_dl[id].push(new sBuffer(pPacket, iPacketLength));
  sem_post(&sem_nw_to_dl[id]);
  return iPacketLength;
}
int nw_to_dl_recv( int id, char ** pPacket ) {
  sem_wait(&sem_nw_to_dl[id]);
  if ( ! nw_to_dl[id].empty() ) {
    sBuffer *buffer = nw_to_dl[id].front();
    nw_to_dl[id].pop();
    *pPacket = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

// Application to Network
int ap_to_nw_send( int id, char * pData, int iDataLength ) {
  ap_to_nw[id].push(new sBuffer(pData, iDataLength));
  sem_post(&sem_ap_to_nw[id]);
  return iDataLength;
}
int ap_to_nw_recv( int id, char ** pData ) {
  sem_wait(&sem_ap_to_nw[id]);
  if ( ! ap_to_nw[id].empty() ) {
    sBuffer *buffer = ap_to_nw[id].front();
    ap_to_nw[id].pop();
    *pData = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

// Network to Application
int nw_to_ap_send( int id, char * pData, int iDataLength ) {
  nw_to_ap[id].push(new sBuffer(pData, iDataLength));
  sem_post(&sem_nw_to_ap[id]);
  return iDataLength;
}
int nw_to_ap_recv( int id, char ** pData ) {
  sem_wait(&sem_nw_to_ap[id]);
  if ( ! nw_to_ap[id].empty() ) {
    sBuffer *buffer = nw_to_ap[id].front();
    nw_to_ap[id].pop();
    *pData = buffer->pBuffer;
    return buffer->iLength;
  }
  return -1;
}

