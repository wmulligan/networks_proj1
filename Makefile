all: server client
 
server: server.o physical_layer.o datalink_layer.o network_layer.o server_app_layer.o queue.o mysql.o
	g++ -pthread -I/usr/local/mysql-current/include -L/usr/local/mysql-current/lib/mysql -lmysqlclient -ldl -o server server.o physical_layer.o datalink_layer.o network_layer.o server_app_layer.o queue.o mysql.o

client: client.o physical_layer.o datalink_layer.o network_layer.o client_app_layer.o queue.o client_func.o
	g++ -pthread -o client client.o physical_layer.o datalink_layer.o network_layer.o client_app_layer.o queue.o client_func.o

server.o: server.cpp
	g++ -c server.cpp

client.o: client.cpp
	g++ -c client.cpp
	
queue.o: queue.cpp queue.h
	g++ -c queue.cpp

client_func.o: client_func.cpp client_func.h
	g++ -c client_func.cpp

server_func.o: server_func.cpp server_func.h
	g++ -c -I/usr/local/mysql-current/include server_func.cpp -L/usr/local/mysql-current/lib/mysql 

physical_layer.o: physical_layer.cpp physical_layer.h
	g++ -c physical_layer.cpp

datalink_layer.o: datalink_layer.cpp datalink_layer.h
	g++ -c datalink_layer.cpp

network_layer.o: network_layer.cpp network_layer.h
	g++ -c network_layer.cpp

server_app_layer.o: server_app_layer.cpp server_app_layer.h
	g++ -c -I/usr/local/mysql-current/include server_app_layer.cpp -L/usr/local/mysql-current/lib/mysql 

client_app_layer.o: client_app_layer.cpp client_app_layer.h
	g++ -c client_app_layer.cpp

mysql.o: mysql.cpp mysqlh.h
	g++ -c -I/usr/local/mysql-current/include  mysql.cpp -L/usr/local/mysql-current/lib/mysql 
clean:
	rm *.o server client
