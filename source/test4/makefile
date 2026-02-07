INCLUDE = -I.. -I/usr/local/include
LIB = -lprotobuf -lmuduo_net -lmuduo_base -lpthread

.PHONY: clean all
all: server publish subscribe
server: server.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
publish: publish_client.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
subscribe: subscribe_client.cpp ../network/pb/message.pb.cc
	g++ $^ -o $@ $(INCLUDE) $(LIB) -g
clean:
	rm -f server publish subscribe
