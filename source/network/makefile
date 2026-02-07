INCLUDE = -I. -I/usr/local/include
LIB = -lprotobuf -lmuduo_net -lmuduo_base -lpthread

.PHONY: clean all
all: server client
server: test_server.cpp pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
client: test_client.cpp pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
clean:
	rm -f server client
