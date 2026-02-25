#include "message.hpp"
#include "network.hpp"
#include "dispatcher.hpp"
#include "Util.hpp"
#include <google/protobuf/struct.pb.h>

void onMessage(const MyRpc::ConnectionBase::ptr& conn, MyRpc::RpcRequest::ptr& msg)
{
    ILOG("%s %zu bytes", "收到RPC请求:", msg->serialize().size());
    MyRpc::RpcResponse::ptr rrq3 = MyRpc::MessageFactory::create<MyRpc::RpcResponse>();
    rrq3->SetId(msg->GetId());
    rrq3->setRcode(MyRpc::Rcode::RCODE_OK);
    rrq3->SetType(MyRpc::Mtype::RSP_RPC);
    google::protobuf::Struct result;
    PbUtil::SetInt(&result, "value", 42);
    rrq3->set_result(result);
    conn->send(rrq3);
}
void onTopic(const MyRpc::ConnectionBase::ptr& conn, MyRpc::TopicRequest::ptr& msg)
{
    ILOG("%s %zu bytes", "收到Topic请求:", msg->serialize().size());
    MyRpc::TopicResponse::ptr rrq3 = MyRpc::MessageFactory::create<MyRpc::TopicResponse>();
    rrq3->SetId(msg->GetId());
    rrq3->setRcode(MyRpc::Rcode::RCODE_OK);
    rrq3->SetType(MyRpc::Mtype::RSP_TOPIC);
    conn->send(rrq3);
}
void onService(const MyRpc::ConnectionBase::ptr& conn, MyRpc::ServiceRequest::ptr& msg)
{
    ILOG("%s %zu bytes", "收到Service请求:", msg->serialize().size());
    MyRpc::ServiceResponse::ptr rrq3 = MyRpc::MessageFactory::create<MyRpc::ServiceResponse>();
    rrq3->SetId(msg->GetId());
    rrq3->setRcode(MyRpc::Rcode::RCODE_OK);
    rrq3->SetType(MyRpc::Mtype::RSP_SERVICE);
    rrq3->setMethod("Add");
    conn->send(rrq3);
}
int main()
{
    MyRpc::ServerBase::ptr server = MyRpc::ServerFactory::create(9000);
    MyRpc::Dispatcher::ptr dispatcher = std::make_shared<MyRpc::Dispatcher>();
    dispatcher->registerHandler<MyRpc::RpcRequest>(MyRpc::Mtype::REQ_RPC, onMessage);
    dispatcher->registerHandler<MyRpc::TopicRequest>(MyRpc::Mtype::REQ_TOPIC, onTopic);
    dispatcher->registerHandler<MyRpc::ServiceRequest>(MyRpc::Mtype::REQ_SERVICE, onService);
    auto onmessage = std::bind(&MyRpc::Dispatcher::messageCallBack, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    server->SetMessageCallBack(onmessage);
    server->start();
    return 0;
}
