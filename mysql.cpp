#include "mysqlh.h"

/* MySQL functions 
 * mysql.cpp
 *   Klevis Luli
 */


/* Function establishes connection to MySQL 
   database and returns handler */
MYSQL*	dbConnect(){
	
	MYSQL *conn; //struct used as mysql connection handler

	//obtain handler
	conn = mysql_init(NULL);

	//check if handler was obtained
	if (conn == NULL) {
     		cout<<"[Application] Error "<<mysql_errno(conn)<<": "<<mysql_error(conn)<<endl;
		exit(1);
 	}

	
	//establish connection to db 
	if ( mysql_real_connect(conn, DBHOST, USER, PASSWORD, DATABASE, 0, 0, 0) == NULL ){
		//exit if it fails
		cout<<"[Application] Error "<<mysql_errno(conn)<<": "<<mysql_error(conn)<<endl;
		exit(-1);
	}
	
	// Return established connection
	return conn;
}
	

/* Close connection to MySQL database */
void dbClose(MYSQL* conn){
	mysql_close(conn);
}
	

