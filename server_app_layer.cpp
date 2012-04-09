/* server_app_layer.cpp
 * Will Mulligan
 * Klevis Luli
 */
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <mysql.h>
#include <string.h>

#include "server_app_layer.h"
#include "server_func.cpp"
#include "mysqlh.h"
#include "queue.h"

using namespace std;

void queryPicture(int sockt, MYSQL* conn);

void updatePicture(int sockt, MYSQL* conn, char* data);

void * ApplicationLayer( void * longPointer )
{
  intptr_t iSocket = (intptr_t) longPointer; // socket handle
  int iRecvLength; // length of recieved data
  int iSendLength; // length of sent data
  MYSQL* mysqlConn = dbConnect(); //connect to db and save handler


  int iDataLength;

  cout << "[Application] Initializing..." << endl;
  
  while ( true )
  {
    char * pData;
   
    
    // Block until data is received from network
    if ( ( iRecvLength = nw_to_ap_recv( iSocket, &pData ) ) == -1 ) {
      cout << "[Application] Error receiving data from network." << endl;
      break;
    }
    cout << "[Application] Received " << iRecvLength << " byte data from network." << endl;
    cout << "[Application] Received: " << pData << endl;

	
    char *pSendData;
    pSendData = (char*)malloc(400*sizeof(char));
    memset(pSendData, 0, sizeof(pSendData));
    if (userType==0){

	strcpy(pSendData,processLogin(pData, mysqlConn));
	//if user has not been authenticated yet
    }
    else {
	//user has logged in
	
	if (isPicture(pData)==1 && userType==1){
		updatePicture(iSocket, mysqlConn, pData);
		continue;
	}
	else if (isPicture(pData)==2){
		queryPicture(iSocket,mysqlConn);
		continue;
	}

	strcpy(pSendData,processCommand(pData,mysqlConn));
    } 
   
    int iDataLength = strlen(pSendData)+1;

    
    cout << "[Application] Sending: " << pData << endl;
    
    // Block until data is sent to network
    if ( ( iSendLength = ap_to_nw_send( iSocket, pSendData, iDataLength ) ) != iDataLength ) {
      cout << "[Application] Error sending data to network." << endl;
      break;
    }
    cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
  }
  
  dbClose(mysqlConn); //close connection to db

  cout << "[Application] Terminating." << endl;
  pthread_exit(NULL);
}



/* query picture, gets picture from db and sends
   it to client
*/
void queryPicture(int sockt, MYSQL* conn){
  MYSQL_RES *result;
  MYSQL_ROW row;
  char reply[100];
  char*  pictureBuffer;
  char query[500];
  int qReturn;
  char tmp[50];
 int iSendLength;

	memset(reply, 0, sizeof(reply));//clean reply buffer
	memset(query, 0, sizeof(query));//clean query buffer
  
	unsigned long *lengths;
	sprintf(query, "SELECT data,filename FROM picture WHERE id=%i",selectID);
  	mysql_query(conn, query);
	result = mysql_store_result(conn);
	
	
	if (result && mysql_num_rows(result) == 1){
		
	  	row = mysql_fetch_row(result);
	  	lengths = mysql_fetch_lengths(result);
		pictureBuffer = (char*) malloc(lengths[0] * sizeof(char)+1);
		reply[0]='1';

		sprintf(tmp, " %s",row[1]);
		strcat(reply, tmp);

	        memcpy(pictureBuffer, row[0], lengths[0]*sizeof(char)+1);

		 int iDataLength = strlen(reply);
    
		    cout << "[Application] Sending: " << reply << endl;
		    
		    // Block until data is sent to network
		    //send first reply with filename
		    if ( ( iSendLength = ap_to_nw_send( sockt, reply, iDataLength ) ) != iDataLength ) {
		      cout << "[Application] Error sending data to network." << endl;
		      exit(1);
		    }
		      cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
	
		  
		   // Block until data is sent to network
		    //send second reply with image data
		    if ( ( iSendLength = ap_to_nw_send( sockt, pictureBuffer, lengths[0]+1 ) ) != lengths[0]+1 ) {
		      cout << "[Application] Error sending data to network." << endl;
		      exit(1);
		    }
		      cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;

	}
	else { 
		reply[0]= '0'; //failed
		strcat(reply," No picture for ID.");

		 int iDataLength = strlen(reply);

		// Block until data is sent to network
		 //send  reply 
		    if ( ( iSendLength = ap_to_nw_send( sockt, reply, iDataLength ) ) != iDataLength ) {
		      cout << "[Application] Error sending data to network." << endl;
		      exit(1);
		    }
		      cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;

	}



  mysql_free_result(result);

}


/* update picture, receives picture from client
   and saves it in the db
*/
void updatePicture(int sockt, MYSQL* conn, char* data){
  char reply[100];
  char *pictureBuffer;
  char chunk[4*1000*1024];
  int fileSize;
  char query[1024*5000];
 int iSendLength;
  char* filename;

	//convert it to string
	istringstream in(data);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}

	strcpy(filename, (char*)words[2].c_str());

	memset(pictureBuffer, 0, sizeof(pictureBuffer));

	// Block until data is received from network
	    if ( ( fileSize = nw_to_ap_recv( sockt, &pictureBuffer ) ) == -1 ) {
	      cout << "[Application] Error receiving data from network." << endl;
	      exit(1);
	    }
	    cout << "[Application] Received " << fileSize << " byte data from network." << endl;
	    cout << "[Application] Received: " << filename << endl;
	


	pictureBuffer[fileSize+1]='\0';


	memset(reply, 0, sizeof(reply));//clean reply buffer
	memset(query, 0, sizeof(query));//clean query buffer
  
	mysql_real_escape_string(conn,chunk,pictureBuffer,fileSize);
	int len;
	sprintf(query, "UPDATE bodies SET data='%s',filename='%s' FROM picture WHERE id=%i",chunk,filename,selectID);
  	if (mysql_query(conn, query)!=0){
		memset(query, 0, sizeof(query));//clean query buffer
		char *stat="INSERT INTO bodies (body_id,data,filename) VALUES (%i,'%s','%s')";
		len=snprintf(query, sizeof(stat)+sizeof(chunk)+sizeof(selectID)+sizeof(filename), stat, selectID,chunk,filename);
	};
	
	if (mysql_real_query(conn, query, len) !=0)
	{

		reply[0]= '0'; //failed
		strcat(reply," Database Error.");

		 int iDataLength = strlen(reply)+1;

		// Block until data is sent to network
		 //send  reply 
		    if ( ( iSendLength = ap_to_nw_send( sockt, reply, iDataLength ) ) != iDataLength ) {
		      cout << "[Application] Error sending data to network." << endl;
		      exit(1);
		    }
		      cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
	

	}
	
	else {
		
	  	
		reply[0]='1';
		strcat(reply," Picture updated.");

	         int iDataLength = strlen(reply)+1;
    
		    cout << "[Application] Sending: " << reply << endl;
		    
		    // Block until data is sent to network
		    //send first reply with filename
		    if ( ( iSendLength = ap_to_nw_send( sockt, reply, iDataLength ) ) != iDataLength ) {
		      cout << "[Application] Error sending data to network." << endl;
		      exit(1);
		    }
		      cout << "[Application] Sent " << iSendLength << " byte data to network." << endl;
	
	}

}


