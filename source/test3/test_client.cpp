#include "../client/rpc_client.hpp"
#include "../network/Util.hpp"

void onResult(const google::protobuf::Struct& result)
{
    std::cout << "CallBack(12+16):The answer is " << PbUtil::GetInt(result.fields().at("value")) << std::endl;
}
void testCommunication()
{
    MyRpc::Client::RpcClient::ptr client = std::make_shared<MyRpc::Client::RpcClient>(true, "127.0.0.1", 9000);
    google::protobuf::Struct para;
    PbUtil::SetInt(&para, "num1", 4);
    PbUtil::SetInt(&para, "num2", 7);
    google::protobuf::Struct ans;
    if (client->call("Add", para, ans))
    {
        std::cout << "Sync(4+7):The answer is " << PbUtil::GetInt(ans.fields().at("value")) << std::endl;
    }
    std::future<google::protobuf::Struct> ans2;
    PbUtil::SetInt(&para, "num1", 7);
    PbUtil::SetInt(&para, "num2", 10);
    if (client->call("Add", para, ans2))
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "ASync(7+10):The answer is " << PbUtil::GetInt(ans2.get().fields().at("value")) << std::endl;
    }
    PbUtil::SetInt(&para, "num1", 12);
    PbUtil::SetInt(&para, "num2", 16);
    client->call("Add", para, onResult);

    sleep(30);
}
int main()
{
    testCommunication();
    return 0;
}
