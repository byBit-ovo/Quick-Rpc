#include "network.hpp"
#include "dispatcher.hpp"
#include "message.hpp"
#include "Util.hpp"
#include <google/protobuf/struct.pb.h>

void RpcMessageTest()
{
    MyRpc::RpcRequest::ptr rrq = std::dynamic_pointer_cast<MyRpc::RpcRequest>(MyRpc::MessageFactory::create(MyRpc::Mtype::REQ_RPC));
    rrq->SetId(Uuid::uuid());
    rrq->setMethod("Add");
    google::protobuf::Struct params;
    PbUtil::SetInt(&params, "num1", 1);
    PbUtil::SetInt(&params, "num2", 4);
    rrq->set_parameters(params);
    std::string msg = rrq->serialize();
    std::cout << msg.size() << " bytes" << std::endl;
    MyRpc::RpcRequest::ptr rrq2 = std::dynamic_pointer_cast<MyRpc::RpcRequest>(MyRpc::MessageFactory::create(MyRpc::Mtype::REQ_RPC));
    rrq2->deserialize(msg);
    std::cout << rrq2->method() << std::endl;
    std::cout << PbUtil::GetInt(rrq2->parameters().fields().at("num2")) << std::endl;
    MyRpc::RpcResponse::ptr rrq3 = MyRpc::MessageFactory::create<MyRpc::RpcResponse>();
    rrq3->SetId(Uuid::uuid());
    rrq3->setRcode(MyRpc::Rcode::RCODE_OK);
    rrq3->SetType(MyRpc::Mtype::RSP_RPC);
    google::protobuf::Value result;
    result.set_number_value(42);
    rrq3->set_result(result);
    rrq3->check();
    std::cout << rrq3->serialize().size() << " bytes" << std::endl;
}
void TopicTest()
{
    MyRpc::TopicRequest::ptr tr = MyRpc::MessageFactory::create<MyRpc::TopicRequest>();
    tr->setTopicKey("Music");
    tr->setOpType(MyRpc::TopicOptype::TOPIC_PUBLISH);
    tr->setTopicMsg("就是爱你");
    std::cout << tr->serialize().size() << " bytes" << std::endl;
    MyRpc::TopicResponse::ptr tr2 = MyRpc::MessageFactory::create<MyRpc::TopicResponse>();
    tr2->SetType(MyRpc::Mtype::RSP_TOPIC);
    tr2->setRcode(MyRpc::Rcode::RCODE_OK);
    std::cout << tr2->serialize().size() << " bytes" << std::endl;
}
void ServiceTest()
{
    MyRpc::ServiceRequest::ptr tr = MyRpc::MessageFactory::create<MyRpc::ServiceRequest>();
    tr->SetType(MyRpc::Mtype::REQ_SERVICE);
    tr->setServiceOpType(MyRpc::ServiceOptype::SERVICE_REGISTRY);
    tr->setHost(std::make_pair("81.71.17.201", 8888));
    tr->setMethod("Design a goolge");
    tr->check();
    std::cout << tr->serialize().size() << " bytes" << std::endl;

    MyRpc::ServiceResponse::ptr trp = MyRpc::MessageFactory::create<MyRpc::ServiceResponse>();
    std::vector<MyRpc::Address> addrs = {{"37.19.293.2", 8980}, {"90.23.11.231", 6789}, {"46.23.45.112", 6565}};
    trp->setHosts(addrs);
    trp->SetType(MyRpc::Mtype::RSP_SERVICE);
    trp->setMethod("Design a google");
    trp->setRcode(MyRpc::Rcode::RCODE_OK);
    trp->setServiceOpType(MyRpc::ServiceOptype::SERVICE_DISCOVERY);
    trp->check();
    std::cout << trp->serialize().size() << " bytes" << std::endl;
}
void onMessage(const MyRpc::ConnectionBase::ptr& conn, MyRpc::RpcResponse::ptr& msg)
{
    ILOG("%s %zu bytes", "收到Message回复:", msg->serialize().size());
}
void onTopic(const MyRpc::ConnectionBase::ptr& conn, MyRpc::TopicResponse::ptr& msg)
{
    ILOG("%s %zu bytes", "收到Topic回复:", msg->serialize().size());
}
void onService(const MyRpc::ConnectionBase::ptr& conn, MyRpc::ServiceResponse::ptr& msg)
{
    ILOG("%s %zu bytes", "收到Service回复:", msg->serialize().size());
}
int main()
{
    MyRpc::Dispatcher::ptr dispatcher = std::make_shared<MyRpc::Dispatcher>();
    dispatcher->registerHandler<MyRpc::RpcResponse>(MyRpc::Mtype::RSP_RPC, onMessage);
    dispatcher->registerHandler<MyRpc::TopicResponse>(MyRpc::Mtype::RSP_TOPIC, onTopic);
    dispatcher->registerHandler<MyRpc::ServiceResponse>(MyRpc::Mtype::RSP_SERVICE, onService);
    MyRpc::ClientBase::ptr client = MyRpc::ClientFactory::create("127.0.0.1", 9000);
    auto onmessage = std::bind(&MyRpc::Dispatcher::messageCallBack, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    client->SetMessageCallBack(onmessage);
    client->connect();
    std::vector<MyRpc::MessageBase::ptr> messages;
    MyRpc::RpcRequest::ptr rrq = std::dynamic_pointer_cast<MyRpc::RpcRequest>(MyRpc::MessageFactory::create(MyRpc::Mtype::REQ_RPC));
    rrq->SetId(Uuid::uuid());
    rrq->setMethod("Add");
    google::protobuf::Struct params;
    PbUtil::SetInt(&params, "num1", 1);
    PbUtil::SetInt(&params, "num2", 4);
    rrq->set_parameters(params);
    rrq->SetType(MyRpc::Mtype::REQ_RPC);
    messages.push_back(std::move(rrq));
    MyRpc::TopicRequest::ptr tr = MyRpc::MessageFactory::create<MyRpc::TopicRequest>();
    tr->setTopicKey("Music");
    tr->SetType(MyRpc::Mtype::REQ_TOPIC);
    tr->setOpType(MyRpc::TopicOptype::TOPIC_PUBLISH);
    tr->setTopicMsg("就是爱你");
    messages.push_back(std::move(tr));
    MyRpc::ServiceRequest::ptr trs = MyRpc::MessageFactory::create<MyRpc::ServiceRequest>();
    trs->SetType(MyRpc::Mtype::REQ_SERVICE);
    trs->setServiceOpType(MyRpc::ServiceOptype::SERVICE_REGISTRY);
    trs->setHost(std::make_pair("81.71.17.201", 8888));
    trs->setMethod("Design a goolge");
    messages.push_back(std::move(trs));
    for (int i = 0; i < 3; ++i)
    {
        client->send(messages[i]);
        sleep(3);
    }
    return 0;
}
