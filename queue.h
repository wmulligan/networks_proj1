/* queue.h
 * Contains functions to send data between layers
 *
 * Author(s):
 * Will Mulligan
 */

void initQueue( void );

void terminateQueue( void );

int ph_to_dl_send( int id, char * pFrame, int iFrameLength );
int ph_to_dl_recv( int id, char ** pFrame );

int dl_to_ph_send( int id, char * pFrame, int iFrameLength );
int dl_to_ph_recv( int id, char ** pFrame );

int dl_to_nw_send( int id, char * pPacket, int iPacketLength );
int dl_to_nw_recv( int id, char ** pPacket );

int nw_to_dl_send( int id, char * pPacket, int iPacketLength );
int nw_to_dl_recv( int id, char ** pPacket );

int ap_to_nw_send( int id, char * pData, int iDataLength );
int ap_to_nw_recv( int id, char ** pData );

int nw_to_ap_send( int id, char * pData, int iDataLength );
int nw_to_ap_recv( int id, char ** pData );
