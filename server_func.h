/* server_func.h
 *
 * Klevis Luli 
 */


#include <string.h>
#include <mysql.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

/* process command received by the client
   and return reply*/
char* processCommand(char *data, MYSQL* conn);

/* process log in and create account commands and 
   return reply */
char* processLogin(char *data, MYSQL* conn);
