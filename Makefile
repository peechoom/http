all: server.o
	g++ server.o -o server
test: http.o
	g++ testServer.cpp http.o -o test
server.o: http.o server.cpp
	g++ -Wall -std=gnu++20 -Wl,--no-as-needed -lpthread http.o server.cpp -c -o server.o
http.o: http.cpp
	g++ -Wall -std=gnu++20 http.cpp -c -o http.o
