#include "../network/pb_message.hpp"
#include "../network/network.hpp"
#include "../network/dispatcher.hpp"
#include "../network/Util.hpp"
#include "../server/rpc_router.hpp"
#include <google/protobuf/struct.pb.h>

void Add(const google::protobuf::Struct& req, google::protobuf::Struct& resp)
{
    int num1 = PbUtil::GetInt(req.fields().at("num1"));
    int num2 = PbUtil::GetInt(req.fields().at("num2"));
    PbUtil::SetInt(&resp, "value", num1 + num2);
}
void testCommunication()
{
    MyRpc::ServerBase::ptr server = MyRpc::ServerFactory::create(10086);
    MyRpc::Dispatcher::ptr dispatcher = std::make_shared<MyRpc::Dispatcher>();
    MyRpc::Server::RpcRouter::ptr router = std::make_shared<MyRpc::Server::RpcRouter>();
    MyRpc::Server::ServiceDescBuilder::ptr serviceBuilder = std::make_shared<MyRpc::Server::ServiceDescBuilder>();
    auto req_rpc = std::bind(&MyRpc::Server::RpcRouter::onRpcRequest, router.get(), std::placeholders::_1, std::placeholders::_2);
    dispatcher->registerHandler<MyRpc::RpcRequest>(MyRpc::Mtype::REQ_RPC, req_rpc);
    auto service1 = serviceBuilder->setMethodName("Add")
                        .setParamsDesc("num1", MyRpc::Server::parameterType::INTEGRAL)
                        .setParamsDesc("num2", MyRpc::Server::parameterType::INTEGRAL)
                        .setReturnType(MyRpc::Server::parameterType::INTEGRAL)
                        .setCallback(Add)
                        .build();
    router->registerService(service1);
    auto message_call = std::bind(&MyRpc::Dispatcher::messageCallBack, dispatcher.get(),
                                  std::placeholders::_1, std::placeholders::_2);
    server->SetMessageCallBack(message_call);
    server->start();
}
int main()
{
    testCommunication();
    return 0;
}
