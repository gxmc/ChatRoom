objects1 = Client.o client.o
objects2 = ThreadPool.o Server.o server.o

vpath %.cpp ../src
vpath %.h ../src

all : client server
.PHONY : all

client : $(objects1)
	g++ -g -std=c++11 -Wall -o client $(objects1) -lpthread

server : $(objects2)
	g++ -g -std=c++11 -Wall -o server $(objects2) -lpthread

.PHONY : clean
clean :
	rm -rf client server *.o
