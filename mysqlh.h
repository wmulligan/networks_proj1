/* MySQL  header file
 * mysqlh.h
 *
 * Klevis Luli 
 */


#include <mysql.h>
#include <iostream>
	


using namespace std;

/* Establishes connection to MySQL 
   database and returns handler */
MYSQL*	dbConnect();

/* Close connection to MySQL database */
void dbClose(MYSQL* conn);
