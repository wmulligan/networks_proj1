/* server_func.cpp
 *
 * Application layer functions 
 * Server
 *
 * Klevis Luli 
 */

#include "server_func.h"



/* process command received by the client
   and return reply*/
char* processCommand(char *data, MYSQL* conn, int *userType, int* selectID){
	char *reply=(char*)malloc(sizeof(char)*1000);
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
				sprintf(query,"SELECT id_number FROM bodies WHERE first_name='%s' and last_name='%s' and location='%s'  ORDER BY id_number DESC LIMIT 1",(char*)words[1].c_str(),(char*)words[2].c_str(),(char*)words[3].c_str());
				qReturn = mysql_query(conn, query);
				if (qReturn != 0) {
				 	//if failed
					cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
					reply[0]= '0';
					strcat(reply, " Mysql Error");
				}

				//fetch result
				result = mysql_store_result(conn);
				
				if (result && mysql_num_rows(result)!=0){	
					 
					row = mysql_fetch_row(result);

					reply[0]= '1';
					sprintf(tmp, " Added. ID: %s",row[0]);
					strcat(reply, tmp);
					*selectID=atoi(row[0]);
				}
				//complete
				mysql_free_result(result);
			}
	    		
		}
		


		else if ((words[0].compare("UNKNOWN") == 0 || words[0].compare("unknown")==0) && words.size()==1){
			memset(reply, 0, sizeof(reply));//clean reply buffer
			memset(query, 0, sizeof(query));//clean reply buffer
			//process UNKNOWN
			//build query				
			sprintf(query,"SELECT id_number,location FROM bodies WHERE status=%i",0);
			qReturn = mysql_query(conn, query);
			if (qReturn != 0) {
					//if failed
					cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
					reply[0]= '0';
					strcat(reply, " Mysql Error");
			}
			else {

				//fetch result
				result = mysql_store_result(conn);
						
				if (result && mysql_num_rows(result)!=0){	
					reply[0]= '1' ;
					strcat(reply, " ");
					while(row = mysql_fetch_row(result)){
						memset(tmp,0,sizeof(tmp));
						sprintf(tmp, " %s:%s\n", row[0],row[1]);
						strcat(reply, tmp);
					}
				}
				else {
				//no unknowns
					reply[0]= '0';
					strcat(reply, " No unidentified bodies.");

				}
		      }
	


			
		}
		
		else if ((words[0].compare("SELECT") == 0 || words[0].compare("select")==0) && words.size()==2){
			//process SELECT id command
			memset(query, 0, sizeof(query));//clean query buffer
			memset(reply, 0, sizeof(reply));//clean reply buffer
			
			sprintf(query, "SELECT * FROM bodies WHERE id_number=%i",atoi(words[1].c_str()) );
			
			//run query		
			qReturn = mysql_query(conn, query);
			if (qReturn != 0) {
				cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
			}
			
			//get result and prepare reply msg
	    		result = mysql_store_result(conn);
	    		if(mysql_num_rows(result) == 1){
				//send success reply
				*selectID = atoi(words[1].c_str()); //update selected id
				memset(tmp,0,sizeof(tmp));
				sprintf(tmp, " Selected ID: %i",*selectID);
				reply[0]= '1';
				strcat(reply, tmp);
	    		}
			else {
				reply[0]= '0'; //failed
				strcat(reply," ID does not exist!");
			}
	 		mysql_free_result(result);

				
		}
		else if (*selectID==1){//if an id has been selected
			if ((words[0].compare("QUERY") == 0 || words[0].compare("query")==0) && words.size()==2){
				memset(query, 0, sizeof(query));//clean query buffer
				memset(reply, 0, sizeof(reply));//clean reply buffer
				if (words[1].compare("STATUS") == 0 || words[1].compare("status")==0) {
					//process QUERY STATUS 
					//build query				
					sprintf(query,"SELECT status FROM bodies WHERE id_number=%i",*selectID);
					qReturn = mysql_query(conn, query);
					if (qReturn != 0) {
						 	//if failed
							cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
							reply[0]= '0';
							strcat(reply, " Mysql Error");
					}
					else {

						//fetch result
						result = mysql_store_result(conn);
						
						if (result && mysql_num_rows(result)!=0){	
							 
							row = mysql_fetch_row(result);

							memset(tmp,0,sizeof(tmp));

							reply[0]= '1';
							row[0]+='\0';
							if (strcmp(row[0],"0")==0)
								sprintf(tmp, " Status: Unidentified.");
							else if (strcmp(row[0],"1")==0)
								sprintf(tmp, " Status: Identified.");

							strcat(reply, tmp);
						}
					      }
				
				}
				//if requesting name
				else if (words[1].compare("NAME") == 0 || words[1].compare("name")==0){
					//process NAME
					//build query				
					sprintf(query,"SELECT first_name,last_name FROM bodies WHERE id_number=%i",*selectID);
					cout<<query<<endl;
					qReturn = mysql_query(conn, query);
					if (qReturn != 0) {
						 	//if failed
							cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
							reply[0]= '0';
							strcat(reply, " Mysql Error");
					}
					else {

						//fetch result
						result = mysql_store_result(conn);
						
						if (result && mysql_num_rows(result)!=0){	
							 
							row = mysql_fetch_row(result);

							memset(tmp,0,sizeof(tmp));

							reply[0]= '1';
							sprintf(tmp, " First Name: %s, Last Name: %s", row[0],row[1]);
							strcat(reply, tmp);
						}
					      }
				



				}
				//if requesting location
				else if (words[1].compare("LOCATION") == 0 || words[1].compare("location")==0){
					//process QUERY LOCATION
					//build query				
					sprintf(query,"SELECT location FROM bodies WHERE id_number=%i",*selectID);
					qReturn = mysql_query(conn, query);
					cout<<"HERE"<<reply<<endl;
					if (qReturn != 0) {
						 	//if failed
							cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
							reply[0]= '0';
							strcat(reply, " Mysql Error");
					}
					else {

						//fetch result
						result = mysql_store_result(conn);
						
						if (result && mysql_num_rows(result)!=0){	
							 
							row = mysql_fetch_row(result);

							memset(tmp,0,sizeof(tmp));

							reply[0]= '1';
							sprintf(tmp, " Location: %s", row[0]);
							strcat(reply, tmp);
						}
					      }
				


	

				}
				
	
				mysql_free_result(result);
			}

			if ((words[0].compare("UPDATE") == 0 || words[0].compare("update")==0) && words.size()>2 && *userType==1){
				memset(query, 0, sizeof(query));//clean query buffer
				memset(reply, 0, sizeof(reply));//clean reply buffer
			
				//if updating status
				if  ((words[1].compare("STATUS") == 0 || words[1].compare("status")==0) && words.size()==3){
				sprintf(query,"UPDATE bodies SET status=%i WHERE id_number=%i",atoi(words[2].c_str()),*selectID);

				}
				//if updating name
				else if ((words[1].compare("NAME") == 0 || words[1].compare("name")==0) && words.size()==4){
				sprintf(query,"UPDATE bodies SET first_name='%s', last_name='%s' WHERE id_number=%i",words[2].c_str(),words[3].c_str(),*selectID);
			
				}
				//if updating location
				else if ((words[1].compare("LOCATION") == 0 || words[1].compare("location")==0) && words.size()==3){
				sprintf(query,"UPDATE bodies SET location='%s' WHERE id_number=%i",words[2].c_str(),*selectID);

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
		
		else if (*selectID==0){
				//no id selected 
				//send reply
				reply[0]= '0';
				strcat(reply, " Select an ID first!");
			}
			
		
	return reply;

}

/* checks if command is update or query picture,
   returns 1 for update, 2 for query */
int isPicture(char *data){
	int picture=0;
	
        //convert it to string
	istringstream in(data);

	//tokenize string to validate commands
	string word;
	vector <string> words;
	  
	while(in >> word) {
	    words.push_back(word);
	}
	


		//if requesting picture
		if ( (words[0].compare("QUERY") == 0 || words[0].compare("query")==0) && (words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0) && words.size()==2 ){
			picture=2;	
		}
		//if updating picture
		else if  ((words[0].compare("UPDATE") == 0 || words[0].compare("update")==0) && (words[1].compare("PICTURE") == 0 || words[1].compare("picture")==0) && words.size()==3){
			picture=1;
		}
	
	return picture;

}
/* process log in and create account commands and 
   return reply */
char* processLogin(char *data, MYSQL* conn,int *userType, int* selectID){
	char reply[1000];	// query to be used for authentication
	char query[500];
	int qReturn;
	MYSQL_RES *result;
	MYSQL_ROW row;

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
		sprintf(query, "SELECT authorized FROM users where username='%s' and password='%s'",(char*)words[1].c_str(),(char*)words[2].c_str());
		//run query		
		qReturn = mysql_query(conn, query);
		if (qReturn != 0) {
			cout<<"[Application] Mysql Error: "<<mysql_error(conn)<<endl;
		}
		memset(reply, 0, sizeof(reply));
		//get result and prepare reply msg
    		result = mysql_store_result(conn);
    		if(result && mysql_num_rows(result) == 1){
			reply[0]= '1';

			//check user type
			row = mysql_fetch_row(result);

			if (strcmp(row[0],"0")==0)
				*userType=2; //query user
			else if (strcmp(row[0],"1")==0)
				*userType=1; //admin user

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
			*userType=2;
		}
 		
		
	}
	
	return reply;	
}



