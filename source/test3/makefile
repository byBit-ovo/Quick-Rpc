INCLUDE = -I.. -I/usr/local/include
LIB = -lprotobuf -lmuduo_net -lmuduo_base -lpthread

.PHONY: clean all
all: server client register
server: test_server.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
client: test_client.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
register: test_register.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
clean:
	rm -f server client register
