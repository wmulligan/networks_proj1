/* server_func.cpp
 *
 * Application layer functions 
 * Server
 *
 * Klevis Luli 
 */

#include "server_func.h"

int userType=0; //user not logged in yet
int selectID=0;	//selected ID

/* process command received by the client
   and return reply*/
char* processCommand(char *data, MYSQL* conn){
	char reply[1000];
	char query[500];
	int qReturn;
	MYSQL_RES *result;
	MYSQL_ROW row;
	MYSQL_FIELD *field; 
	//prepare reply msg
	char tmp[100];

	//convert it to string
	istringstream in(data);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}

	//check for all possible commands 
		if ((words[0].compare("add") == 0 || words[0].compare("ADD")==0) && words.size()==4){
			memset(reply, 0, sizeof(reply));//clean reply buffer
			memset(query, 0, sizeof(query));//clean query buffer

			//prepare query to create account
			sprintf(query, "INSERT INTO bodies (first_name, last_name, location) VALUES ('%s','%s','%s')",(char*)words[1].c_str(),(char*)words[2].c_str(),(char*)words[3].c_str());

			//run query		
			qReturn = mysql_query(conn, query);
			if (qReturn != 0) {
			 //if failed
				cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
				reply[0]= '0';
				strcat(reply, " Mysql Error");
			}
		
			//prepare success reply msg
			else {
				memset(query, 0, sizeof(query));//clean query buffer
				//get id of inserted body
				sprintf(query,"SELECT id_number FROM BODIES WHERE first_name='%s' and last_name='%s' and location='%s'",(char*)words[1].c_str(),(char*)words[2].c_str(),(char*)words[3].c_str());
				qReturn = mysql_query(conn, query);
				//fetch result
				result = mysql_store_result(conn);
					
				
				//complete
				mysql_free_result(result);
			}
	    		
		}
		
		if ((words[0].compare("SELECT") == 0 || words[0].compare("select")==0) && words.size()==2){
			//process SELECT id command
			memset(query, 0, sizeof(query));//clean query buffer
			memset(reply, 0, sizeof(reply));//clean reply buffer
			
			sprintf(query, "SELECT * FROM bodies WHERE id_number=%i",atoi(words[1].c_str()) );
			
			//run query		
			qReturn = mysql_query(conn, query);
			if (qReturn != 0) {
				cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
			}
			//segfault
			//get result and prepare reply msg
	    		result = mysql_store_result(conn);
	    		if(mysql_num_rows(result) == 1){
				//send success reply
				selectID = atoi(words[1].c_str()); //update selected id
				memset(tmp,0,sizeof(tmp));
				sprintf(tmp, " Selected ID: %i",selectID);
				reply[0]= '1';
				strcat(reply, tmp);
	    		}
			else {
				reply[0]= '0'; //failed
				strcat(reply," ID does not exist!");
			}
	 		mysql_free_result(result);

				
		}
		if (selectID==1){//if an id has been selected
			if ((words[0].compare("QUERY") == 0 || words[0].compare("query")==0) && words.size()==2){
				memset(query, 0, sizeof(query));//clean query buffer
				memset(reply, 0, sizeof(reply));//clean reply buffer
				if (words[1].compare("STATUS") == 0 || words[1].compare("status")==0) {
				//process QUERY STATUS update
				//build query				
				sprintf(query,"SELECT status FROM BODIES WHERE id_number=%i",selectID);
				qReturn = mysql_query(conn, query);
				//fetch result
				result = mysql_store_result(conn);
			    	row = mysql_fetch_row(result);
				
				memset(tmp,0,sizeof(tmp));
				/*if (row[0]==0)
					sprintf(tmp, " Unidentified.");
				else if (row[0]==1)
					sprintf(tmp, " Identified.");
				*/
				reply[0]= '1';
				strcat(reply, tmp);
					
					
				}
				//if requesting name
				else if (words[1].compare("NAME") == 0 || words[1].compare("name")==0){




				}
				//if requesting location
				else if (words[1].compare("LOCATION") == 0 || words[1].compare("location")==0){



				}
				
	
				mysql_free_result(result);
			}

			if ((words[0].compare("UPDATE") == 0 || words[0].compare("update")==0) && words.size()>2){
				memset(query, 0, sizeof(query));//clean query buffer
				memset(reply, 0, sizeof(reply));//clean reply buffer
			
				//if updating status
				if  ((words[1].compare("STATUS") == 0 || words[1].compare("status")==0) && words.size()==3){
				sprintf(query,"UPDATE bodies SET status=%i WHERE id_number=%i",atoi(words[2].c_str()),selectID);

				}
				//if updating name
				else if ((words[1].compare("NAME") == 0 || words[1].compare("name")==0) && words.size()==4){
				sprintf(query,"UPDATE bodies SET first_name='%s', last_name='%s' WHERE id_number=%i",words[2].c_str(),words[3].c_str(),selectID);
			
				}
				//if updating location
				else if ((words[1].compare("LOCATION") == 0 || words[1].compare("location")==0) && words.size()==3){
				sprintf(query,"UPDATE bodies SET location='%s' WHERE id_number=%i",words[2].c_str(),selectID);

				}

				

				//run query		
				qReturn = mysql_query(conn, query);
				if (qReturn != 0) {
			 	//if failed
					cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
					reply[0]= '0';
					strcat(reply, " Mysql Error");
				}
		
				//prepare success reply msg
				reply[0]= '1';
			}
		}
		else if (selectID==0){
				//no id selected 
				//send reply
				reply[0]= '0';
				strcat(reply, " Select an ID first!");
			}


		if ((words[0].compare("UNKNOWN") == 0 || words[0].compare("unknown")==0) && words.size()==1){
			
		}
			
		
	return reply;

}

/* checks if command is update pr query picture,
   returns 1 for update, 2 for query */
int isPicture(char *data){
	int picture=0;
	//convert it to string
	istringstream in(data);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back( word );
	}

	//if requesting picture
	if ( (words[0].compare("QUERY") == 0 || words[0].compare("query")==0) && (words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0)&& words.size()==2 ){
		picture=2;	
	}
	//if updating picture
	else if ((words[0].compare("QUERY") == 0 || words[0].compare("query")==0) || (words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0) && words.size()==3){
		picture=1;
	}
	
	return picture;

}
/* process log in and create account commands and 
   return reply */
char* processLogin(char *data, MYSQL* conn){
	char reply[1000];	// query to be used for authentication
	char query[500];
	int qReturn;
	MYSQL_RES *result;

	//convert it to string
	istringstream in(data);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	
	while(in >> word) {
	    words.push_back( word );
	}
		
		

	  
	//check for login or create account command
	if ((words[0].compare("login") == 0 || words[0].compare("LOGIN")==0) && words.size()==3){
		// query to be used for authentication
		sprintf(query, "SELECT * FROM users where username='%s' and password='%s'",(char*)words[1].c_str(),(char*)words[2].c_str());
		//run query		
		qReturn = mysql_query(conn, query);
		if (qReturn != 0) {
			cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
		}
		memset(reply, 0, sizeof(reply));
		//get result and prepare reply msg
    		result = mysql_store_result(conn);
    		if(mysql_num_rows(result) == 1){
			reply[0]= '1';
			userType=1;
    		}
		else {
			reply[0]= '0'; //failed
			strcat(reply," Incorrect username or password!");
		}
 		mysql_free_result(result);
		
			
	}
	if ((words[0].compare("create") == 0 || words[0].compare("CREATE") == 0) && (words[1].compare("account") == 0 || words[1].compare("ACCOUNT") == 0) && words.size()==4){

	//prepare query to create account
	sprintf(query, "INSERT INTO users (username, password, authorized) VALUES ('%s','%s',0)",(char*)words[2].c_str(),(char*)words[3].c_str());
		
		memset(reply, 0, sizeof(reply));//clean reply buffer

		//run query		
		qReturn = mysql_query(conn, query);
		if (qReturn != 0) {
		//if failed
			cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
			reply[0]= '0';
			strcat(reply, " Mysql Error");
		}
		
		//prepare success reply msg
		else {
			reply[0]= '1';
			strcat(reply, " Account Created.");
		}
 		
		
	}
	
	return reply;	
}



