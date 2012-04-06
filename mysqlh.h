/* MySQL  header file
 * mysqlh.h
 *
 * Klevis Luli 
 */


#include <mysql.h>
#include <iostream>
	
#define DBHOST "mysql.wpi.edu"
#define USER "klevis"
#define PASSWORD "MJDyRB"
#define DATABASE "cs4516_proj1"

using namespace std;

/* Establishes connection to MySQL 
   database and returns handler */
MYSQL*	dbConnect();

/* Close connection to MySQL database */
void dbClose(MYSQL* conn);
