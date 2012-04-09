#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/ioctl.h>

#include "queue.h"
#include "physical_layer.h"
#include "datalink_layer.h"
#include "network_layer.h"
#include "client_app_layer.h"

using namespace std;

void sighandler(int sig);

int iSocket; // socket handle

int main(int argc, char *argv[])
{
  struct sockaddr_in sServerSockAddr; // server socket address
  int iClientSocket; // client socket handle
  pthread_t iPhysicalThreadId; // physical layer thread id
  pthread_t iDataLinkThreadId; // physical layer thread id
  pthread_t iNetworkThreadId; // physical layer thread id
  pthread_t iApplicationThreadId; // physical layer thread id
  
  signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);
  
  cout << "[Client] Initializing..." << endl;
  
  // Initialize Socket
  if ( ( iSocket = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
    cout << "[Client] Error initializing socket." << endl;
    exit(1);
  }
  
  // Initialize Server Address
  sServerSockAddr.sin_family      = AF_INET;
  sServerSockAddr.sin_port        = htons(4000); // todo: fix port num
  sServerSockAddr.sin_addr.s_addr = htonl(INADDR_ANY); // todo: fix address
  
  // Connect to Socket
  if ( connect( iSocket, (struct sockaddr *) &sServerSockAddr, sizeof( sServerSockAddr ) ) == -1 ) {
      cout << "[Client] Error connecting to socket." << endl;
      exit(1);
  }
  cout << "[Client] Connected to server." << endl;
  
  int opt = 1;
  ioctl(iSocket, FIONBIO, &opt);
  
  // Create queues
  initQueue( iSocket );

  // Create Physical Layer Thread
  pthread_create(&iPhysicalThreadId, NULL, &PhysicalLayer, (void *) iSocket);
  pthread_create(&iDataLinkThreadId, NULL, &DataLinkLayer, (void *) iSocket);
  pthread_create(&iNetworkThreadId, NULL, &NetworkLayer, (void *) iSocket);
  pthread_create(&iApplicationThreadId, NULL, &ApplicationLayer, (void *) iSocket);
  pthread_join(iPhysicalThreadId, NULL);
  pthread_join(iDataLinkThreadId, NULL);
  pthread_join(iNetworkThreadId, NULL);
  pthread_join(iApplicationThreadId, NULL);
  
  //wait for command
  cout << "[Client]";

  cout << "[Client] Terminating." << endl;
  close(iSocket);
  exit(0);
}

void sighandler(int sig)
{
  cout << "[Client] Signal Caught. Closing socket connection." << endl;
  //terminateQueue();
  close(iSocket);
  //exit(1);
}


