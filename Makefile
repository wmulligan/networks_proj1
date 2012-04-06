all: server client

server: server.o physical_layer.o datalink_layer.o network_layer.o server_app_layer.o queue.o
	g++ -pthread -o server server.o physical_layer.o datalink_layer.o network_layer.o server_app_layer.o queue.o

client: client.o physical_layer.o datalink_layer.o network_layer.o client_app_layer.o queue.o
	g++ -pthread -o client client.o physical_layer.o datalink_layer.o network_layer.o client_app_layer.o queue.o

server.o: server.cpp
	g++ -c server.cpp

client.o: client.cpp
	g++ -c client.cpp
	
queue.o: queue.cpp queue.h
	g++ -c queue.cpp

physical_layer.o: physical_layer.cpp physical_layer.h
	g++ -fpermissive -c physical_layer.cpp

datalink_layer.o: datalink_layer.cpp datalink_layer.h
	g++ -fpermissive -c datalink_layer.cpp

network_layer.o: network_layer.cpp network_layer.h
	g++ -fpermissive -c network_layer.cpp

server_app_layer.o: server_app_layer.cpp server_app_layer.h
	g++ -fpermissive -c server_app_layer.cpp

client_app_layer.o: client_app_layer.cpp client_app_layer.h
	g++ -fpermissive -c client_app_layer.cpp

clean:
	rm *.o server client
