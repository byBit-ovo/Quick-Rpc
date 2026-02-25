#include "../server/rpc_server.hpp"
#include "../network/Util.hpp"

void Add(const google::protobuf::Struct& para, google::protobuf::Struct& ans)
{
    int num1 = PbUtil::GetInt(para.fields().at("num1"));
    int num2 = PbUtil::GetInt(para.fields().at("num2"));
    PbUtil::SetInt(&ans, "value", num1 + num2);
}
void testCommunication()
{
    MyRpc::Address host = {"127.0.0.1", 10086};
    MyRpc::Address host_register = {"127.0.0.1", 9000};
    MyRpc::Server::RpcServer::ptr server = std::make_shared<MyRpc::Server::RpcServer>(host, true, host_register);
    MyRpc::Server::ServiceDescBuilder::ptr builder = std::make_shared<MyRpc::Server::ServiceDescBuilder>();
    MyRpc::Server::ServiceDesc::ptr add =
        builder->setCallback(Add)
            .setMethodName("Add")
            .setParamsDesc("num1", MyRpc::Server::parameterType::INTEGRAL)
            .setParamsDesc("num2", MyRpc::Server::parameterType::INTEGRAL)
            .setReturnType(MyRpc::Server::parameterType::INTEGRAL)
            .build();
    server->registerService(add);
    server->start();
}
int main()
{
    testCommunication();
    return 0;
}
