#include "../network/pb_message.hpp"
#include "../network/network.hpp"
#include "../network/dispatcher.hpp"
#include "../network/Util.hpp"
#include "../client/rpc_caller.hpp"
#include <google/protobuf/struct.pb.h>
#include <thread>
void testCommunication()
{
    MyRpc::ClientBase::ptr client = MyRpc::ClientFactory::create("127.0.0.1", 10086);
    MyRpc::Client::Requestor::ptr requestor = std::make_shared<MyRpc::Client::Requestor>();
    MyRpc::Client::RpcCaller::ptr caller = std::make_shared<MyRpc::Client::RpcCaller>(requestor);
    MyRpc::Dispatcher::ptr dispatcher = std::make_shared<MyRpc::Dispatcher>();
    auto rpc_rsp_call = std::bind(&MyRpc::Client::Requestor::onResponse, requestor.get(),
                                  std::placeholders::_1, std::placeholders::_2);
    dispatcher->registerHandler<MyRpc::MessageBase>(MyRpc::Mtype::RSP_RPC, rpc_rsp_call);
    auto call_back = std::bind(&MyRpc::Dispatcher::messageCallBack, dispatcher.get(),
                               std::placeholders::_1, std::placeholders::_2);
    client->SetMessageCallBack(call_back);
    client->connect();
    MyRpc::ConnectionBase::ptr conn = client->connection();

    google::protobuf::Struct para;
    google::protobuf::Struct result;
    PbUtil::SetInt(&para, "num1", 12);
    PbUtil::SetInt(&para, "num2", 25);
    if (caller->call(conn, "Add", para, result) != false)
    {
        std::cout << PbUtil::GetInt(result.fields().at("value")) << std::endl;
        sleep(1);
    }

    auto jsonCall = [](const google::protobuf::Struct& result)
    {
        std::cout << "The result is " << PbUtil::GetInt(result.fields().at("value")) << std::endl;
    };
    PbUtil::SetInt(&para, "num1", 4);
    PbUtil::SetInt(&para, "num2", 5);
    if (caller->call(conn, "Add", para, jsonCall) == false)
    {
        std::cout << "回调调用错误" << std::endl;
    }

    std::future<google::protobuf::Struct> fu;
    PbUtil::SetInt(&para, "num1", 6);
    PbUtil::SetInt(&para, "num2", 8);
    if (caller->call(conn, "Add", para, fu) != false)
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << PbUtil::GetInt(fu.get().fields().at("value")) << std::endl;
    }
    conn->shutDown();
}
int main()
{
    testCommunication();
    return 0;
}
