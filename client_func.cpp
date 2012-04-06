/* client_func.cpp
 * Application layer functions 
 * Client
 * Klevis Luli 
 */

#include "client_func.h"


void app_welcomeMsg(void){

	cout << "Disaster Identification System"<<endl;
	cout << "Please login or create a new account to access the database."<<endl;
}


/*String manipulations to make sure input is valid
  returns 1 if valid, 0 if invalid, 2 if image upload, 5 if blank */
int validateInput(char *input){
	int valid = 0;
	
	//check if input is empty or null
	if (input == NULL || input =="" ){
		valid=5;		
		return valid;
	}
	

	//convert it to string
	istringstream in(input);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}

	//check for all possible commands 
	if (words.size()>=2){
		if ((words[0].compare("add") == 0 || words[0].compare("ADD")==0) && words.size()==4)
			valid=1;
		
		if ((words[0].compare("SELECT") == 0 || words[0].compare("select")==0) && words.size()==2)
			valid=1;

		if ((words[0].compare("QUERY") == 0 || words[0].compare("query")==0) && words.size()==2){
			if ((words[1].compare("STATUS") == 0 || words[1].compare("status")==0) ||
			    (words[1].compare("NAME") == 0 || words[1].compare("name")==0) ||
			    (words[1].compare("LOCATION") == 0 || words[1].compare("location")==0) )
				valid=1;

			//if requesting picture
			if (words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0)
				valid=2;
		}

		if ((words[0].compare("UPDATE") == 0 || words[0].compare("update")==0) && words.size()>2){
			if ( ((words[1].compare("STATUS") == 0 || words[1].compare("status")==0) && words.size()==3)||
			     ((words[1].compare("NAME") == 0 || words[1].compare("name")==0) && words.size()==4) ||
			     ((words[1].compare("LOCATION") == 0 || words[1].compare("location")==0) && words.size()==3))
				valid=1;

			//if updating picture
			if  ((words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0) && words.size()==3)
				valid=3;
		}

		if ((words[0].compare("UNKNOWN") == 0 || words[0].compare("unknown")==0) && words.size()==1){
				valid=1;
		}
			
		
	}
	//return valid or invalid flag
	return valid;	

}

/*Validate cmds entered before user is logged in
  returns 1 if valid, 0 if invalid, 5 if blank*/
int validateLogin(char *input){
	int valid = 0;
	
	//check if input is empty or null
	if (input == NULL || input =="" ){
		valid=5; //null or empty input
		return valid;
	}

	//convert it to string
	istringstream in(input);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}
	  
	//check for login or create account command
	if (words.size()>=3){
		if ((words[0].compare("login") == 0 || words[0].compare("LOGIN")==0) && words.size()==3){
			valid=1;
			
		}
		if ((words[0].compare("create") == 0 || words[0].compare("CREATE") == 0) && (words[1].compare("account") == 0 || words[1].compare("ACCOUNT") == 0) && words.size()==4)
			valid=1;
	}

	//return valid or invalid flag
	return valid;	
}

/* Subtracts the second part of a server 
   reply message ([1 XXXXXXXXX]) and returns it */
char* getServerReply(char *reply){
	char sMsg[strlen(reply)];
	memset(sMsg, 0, strlen(reply));
	//check if reply is empty or null
	if (reply == NULL || reply =="")
		return "Invalid Reply";
	string replym;
	istringstream msg(reply);
	
	//subtract message
	msg >> replym;
	while(msg >> replym) {
	    strcat(sMsg, (char *)replym.c_str());
	    strcat(sMsg, " ");
	}
	

	return sMsg;
}

/* Subtracts the file	return valid;name from UPDATE PICTURE command
   returns char array */
char* getFilename(char *input){
	
	
	istringstream in(input);
	
	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}

	return (char *)words[2].c_str();
}


