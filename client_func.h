/*func.h 
 *Contains functions to complete the functionality of 
 * the application layer 
 *
 * Klevis Luli 
 */
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

//client welcome msg
void app_welcomeMsg(void);

/*String manipulations to make sure input is valid
  returns 1 if valid, 0 if invalid, 2 if image query, 3 if upload */
int validateInput(char *input);

/*Validate cmds entered before user is logged in
  returns 1 if valid, 0 if invalid*/
int validateLogin(char *input);

/* Subtracts the second part of a server 
   reply message ([1 XXXXXXXXX]) and returns it */
char* getServerReply(char *reply);

/* Subtracts the filename from UPDATE PICTURE command
   returns char array */
char* getFilename(char *input);

