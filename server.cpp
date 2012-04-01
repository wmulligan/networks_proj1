#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#include "queue.h"
#include "physical_layer.h"
#include "datalink_layer.h"
#include "network_layer.h"
#include "server_app_layer.h"

using namespace std;

void sighandler(int sig);

int iSocket; // socket handle

int main(int argc, char *argv[])
{
  struct sockaddr_in sServerSockAddr; // server socket address
  struct sockaddr_in sClientSockAddr; // client socket address
  socklen_t sClientSockLen; // length of client socket address
  
  signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);
  
  cout << "[Server] Initializing..." << endl;
  
  initQueue();
  
  // Initialize Socket
  if ( ( iSocket = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
    cout << "[Server] Error initializing socket." << endl;
    exit(1);
  }
  
  // Initialize Server Address
  sServerSockAddr.sin_family      = AF_INET;
  sServerSockAddr.sin_port        = htons(4000); // todo: fix port num
  sServerSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  // Bind to Socket
  if ( bind( iSocket, (struct sockaddr *) &sServerSockAddr, sizeof( sServerSockAddr ) ) == -1 ) {
      cout << "[Server] Error binding to socket." << endl;
      exit(1);
  }
  
  // Listen to Socket
  if ( listen( iSocket, 5 ) == -1 ) {
      cout << "[Server] Error listening to socket." << endl;
      exit(1);
  }
  
  cout << "[Server] Waiting for clients..." << endl;
  while ( true )
  {
    int iClientSocket; // client socket handle
    pthread_t iThreadId; // physical layer thread id

    // Accept connections to clients
    sClientSockLen = sizeof(sClientSockAddr);
    if ( ( iClientSocket = accept( iSocket, (struct sockaddr *) &sClientSockAddr, &sClientSockLen ) ) == -1 ) {
      cout << "[Server] Error accepting new connections." << endl;
      break;
    }
    
    // Create Physical Layer Thread
    pthread_create(&iThreadId, NULL, &PhysicalLayer, (void *) iClientSocket);
    pthread_detach(iThreadId);
    pthread_create(&iThreadId, NULL, &DataLinkLayer, (void *) iClientSocket);
    pthread_detach(iThreadId);
    pthread_create(&iThreadId, NULL, &NetworkLayer, (void *) iClientSocket);
    pthread_detach(iThreadId);
    pthread_create(&iThreadId, NULL, &ApplicationLayer, (void *) iClientSocket);
    pthread_detach(iThreadId);
  }
  
  cout << "[Server] Terminating." << endl;
  terminateQueue();
  close(iSocket);
}

void sighandler(int sig)
{
  cout << "[Server] Signal Caught. Closing socket connection." << endl;
  close(iSocket);
}


