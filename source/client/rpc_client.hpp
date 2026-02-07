#pragma once
#include "../network/dispatcher.hpp"
#include "rpc_caller.hpp"
#include "service_manager.hpp"
#include "topic.hpp"
#include "../network/message.hpp"
#include <google/protobuf/struct.pb.h>

namespace MyRpc{
    namespace Client{
        //服务注册客户端
        class RegisterClient{
            public:
                using ptr = std::shared_ptr<RegisterClient>;
                //构造函数中连接注册中心...
                RegisterClient(const std::string &ip, int port):
                _requestor(std::make_shared<Requestor>()),
                _provider(std::make_shared<Provider>(_requestor)),
                _dispatcher(std::make_shared<Dispatcher>()){
                    auto rpc_rsp_call = std::bind(&Client::Requestor::onResponse,_requestor.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //向dispatcher注册 '服务注册响应' 的回调 
                    _dispatcher->registerHandler<MessageBase>(Mtype::RSP_SERVICE,rpc_rsp_call);
                    auto call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _client = ClientFactory::create(ip,port);
                    _client->SetMessageCallBack(call_back);
                }
                void connect(){
                    _client->connect();
                }
                //向注册中心注册服务
                bool registerMethod(const std::string &method,const Address& host){
                    if(_client->connected() == false){
                        ELOG("连接尚未建立!");
                        return false;
                    }
                    return _provider->registerMethod(_client->connection(),method,host);
                }
            private:
                Requestor::ptr _requestor;      // communication core
                Provider::ptr _provider;        //通过_requestor发送服务注册请求 
                Dispatcher::ptr _dispatcher;    //通过_requestor中的onResponse接收响应
                ClientBase::ptr _client;        //has a connection
        };
        //服务发现客户端
        class DiscoverClient{
            public:
                using ptr = std::shared_ptr<DiscoverClient>;
                //构造函数中连接注册中心...
                DiscoverClient() = default;
                DiscoverClient(const std::string &ip, int port,const Discoverer::OfflineCallBack &off_call):
                _requestor(std::make_shared<Requestor>()),
                _discoverer(std::make_shared<Discoverer>(_requestor,off_call)),
                _dispatcher(std::make_shared<Dispatcher>()){
                    //向dispatchetr注册 '发现者收到发现响应' 的回调
                    auto rpc_rsp_call = std::bind(&Requestor::onResponse,_requestor.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _dispatcher->registerHandler<MessageBase>(Mtype::RSP_SERVICE,rpc_rsp_call);

                    //向dispatcher注册 '发现者收到服务上下线请求' 的回调
                    auto line_req = std::bind(&Discoverer::onServiceRequest,_discoverer.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _dispatcher->registerHandler<ServiceRequest>(Mtype::REQ_SERVICE,line_req);

                    auto call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _client = ClientFactory::create(ip,port);
                    _client->SetMessageCallBack(call_back);
                    _client->connect();
                }
                //向注册中心发现服务
                bool serviceDiscover(const std::string &method, Address &host){
                    if(_client->connected() == false){
                        ELOG("连接尚未建立!");
                        return false;
                    }
                    return _discoverer->serviceDiscover(_client->connection(),method,host);
                }
            private:
                Requestor::ptr _requestor;       //communication core
                Discoverer::ptr _discoverer;     //通过_requestor发送服务发现请求
                Dispatcher::ptr _dispatcher;     //通过_requestor中的onResponse接收请求
                ClientBase::ptr _client;         //has a connction

        };

        //模式一：Rpc调用客户端
        //模式二：Rpc服务发现与Rpc调用客户端 
        class RpcClient{
            public:
                using ptr = std::shared_ptr<RpcClient>;
                //构造函数中连接注册中心...,选择是否启用服务发现功能
                //同时决定该ip是服务提供者地址还是服务发现的地址
                RpcClient(bool enable_discover,const std::string &ip, int port):
                _enable_discover(enable_discover),_requestor(std::make_shared<Requestor>()),
                _caller(std::make_shared<RpcCaller>(_requestor)),
                _dispatcher(std::make_shared<Dispatcher>()){
                    auto rpc_rsp_call = std::bind(&Requestor::onResponse,_requestor.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //向dispatchetr注册 'Rpc调用者收到Rpc响应' 的回调
                    _dispatcher->registerHandler<MessageBase>(Mtype::RSP_RPC,rpc_rsp_call);

                    if(_enable_discover == true){
                        //该客户端在内部已经注册requestor的回调，所以不需要再注册,但需要向Discoverer传入收到下线通知后的移除连接的回调
                        auto off_line_call = std::bind(&RpcClient::removeRpcClient,this,std::placeholders::_1);
                        _discover_client = std::make_shared<DiscoverClient>(ip,port,off_line_call); //地址为注册中心
                    }else{
                        //但这里是Muduo客户端，需要注册回调
                        _rpc_client = ClientFactory::create(ip,port);                 //地址为Rpc提供者
                        auto call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                        std::placeholders::_1,std::placeholders::_2);
                        _rpc_client->SetMessageCallBack(call_back);
                        _rpc_client->connect();
                    }
                   
                }
                void shutDown(){

                }
                //向注册中心发现服务,不需要，直接在内部调用，上层只需要调用服务
                // bool serviceDiscover(const std::string &method, Address &host){
                //     //discover 内部会建立method: hosts的关系，所以发现过的服务，直接返回地址进行Rpc调用，不用再次发起发现请求
                //     bool ret = _discover_client->serviceDiscover(method,host);
                //     // putRpcClient(host,ClientFactory::create(host.first,host.second));
                //     putRpcClient(host,newRpcClient(host));

                //     return ret;
                // }
                //同步调用
                bool call(const std::string& method, const google::protobuf::Struct& parameters, google::protobuf::Value& result){
                    ClientBase::ptr rpcClient = getAvailableClient(method);
                    if(rpcClient.get() == nullptr){
                        return false;
                    }
                    return _caller->call(rpcClient->connection(),method,parameters,result);
                }
                //异步调用
                bool call(const std::string& method, const google::protobuf::Struct& parameters, std::future<google::protobuf::Value>& result){
                    ClientBase::ptr rpcClient = getAvailableClient(method);
                    if(rpcClient.get() == nullptr){
                        return false;
                    }
                    return _caller->call(rpcClient->connection(),method,parameters,result);
                }
                //设置回调，调用
                bool call(const std::string& method, const google::protobuf::Struct& parameters, const RpcCaller::PbCallBack& call_back){
                    ClientBase::ptr rpcClient = getAvailableClient(method);
                    if(rpcClient.get() == nullptr){
                        return false;
                    }
                    return _caller->call(rpcClient->connection(),method,parameters,call_back);
                }
            private:
                ClientBase::ptr getAvailableClient(const std::string &method){
                    ClientBase::ptr rpcClient;
                    if(_enable_discover){
                        Address host;
                        bool ret = _discover_client->serviceDiscover(method,host);
                        if(ret == false){
                            ELOG("服务发现失败：尚未有服务提供者提供该服务...");
                            return ClientBase::ptr();
                        }
                        rpcClient = getRpcClient(host);
                        if(rpcClient.get() == nullptr){
                            rpcClient = newRpcClient(host);
                            putRpcClient(host,rpcClient);
                        }
                    }
                    else
                    {
                        rpcClient = _rpc_client;
                    }
                    return rpcClient;
                }
                ClientBase::ptr newRpcClient(const Address &host){
                    //这里是Muduo客户端，需要注册回调
                    ClientBase::ptr client = ClientFactory::create(host.first,host.second);
                    auto call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                        std::placeholders::_1,std::placeholders::_2);
                    client->SetMessageCallBack(call_back);
                    client->connect();
                    return client;
                }
                ClientBase::ptr getRpcClient(const Address &host){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto iter = _rpc_clients.find(host);
                    if(iter==_rpc_clients.end()){
                        return ClientBase::ptr();
                    }
                    return iter->second;
                }
                ClientBase::ptr putRpcClient(const Address &host, const ClientBase::ptr &client){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto pair = _rpc_clients.insert(std::make_pair(host,client));
                    return _rpc_clients[host];
                }
                void removeRpcClient(const Address &host){
                    std::unique_lock<std::mutex> guard(_mutex);
                    _rpc_clients.erase(host);
                }
            private:
                struct AddrHash {
                    std::size_t operator()(const Address &host)const {
                        std::string addr = host.first + std::to_string(host.second);
                        return std::hash<std::string>{}(addr); 
                    }
                };

                bool _enable_discover;
                Requestor::ptr _requestor;             //发送请求的接口,保存请求描述，根据请求描述来接收响应
                DiscoverClient::ptr _discover_client;
                ClientBase::ptr _rpc_client;           //如果没有启用服务发现，则使用此客户端进行Rpc调用
                RpcCaller::ptr _caller;                //发起Rpc请求
                Dispatcher::ptr _dispatcher;           //根据消息类型选择不同的回调进行处理
                std::mutex _mutex;                     
                // std::unordered_map<std::string, std::vector<ClientBase::ptr>> _rpc_clients;
                std::unordered_map<Address, ClientBase::ptr,AddrHash> _rpc_clients;  //如果启用了服务发现，则建立该Rpc客户端连接池

        };

        class TopicClient{
            public:
                using ptr = std::shared_ptr<TopicClient>;
                TopicClient(const std::string &ip,int port):
                _requestor(std::make_shared<Requestor>()),
                _dispatcher(std::make_shared<Dispatcher>()),
                _manager(std::make_shared<TopicManager>(_requestor)),
                _client(ClientFactory::create(ip,port))
                {
                    //向dispatchetr注册 'Topic调用者收到Topic响应' 的回调
                    auto topic_rsp_call = std::bind(&Requestor::onResponse,_requestor.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _dispatcher->registerHandler<MessageBase>(Mtype::RSP_TOPIC,topic_rsp_call);

                    //向dispatchetr注册 'Topic订阅者收到Topic推送消息的请求' 的回调
                    auto topic_handler = std::bind(&TopicManager::onPublish,_manager.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _dispatcher->registerHandler<TopicRequest>(Mtype::REQ_TOPIC,topic_handler);

                    auto call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                        std::placeholders::_1,std::placeholders::_2);
                    _client->SetMessageCallBack(call_back);
                    _client->connect();

                }
                bool createTopic(const std::string &name){
                    return _manager->createTopic(_client->connection(),name);
                }
                bool removeTopic(const std::string &name){
                    return _manager->removeTopic(_client->connection(),name);
                }
                bool cancelTopic(const std::string &name){
                    return _manager->cancelTopic(_client->connection(),name);
                }

                bool publish(const std::string &topic_name,const std::string &msg){
                    return _manager->publish(_client->connection(),topic_name,msg);
                }
                //subscribe topic and regsiter handler to handle it 
                bool subscribeTopic(const std::string &name,const TopicManager::Msg_Func_t &handler){ 
                    return _manager->subscribeTopic(_client->connection(),name,handler);
                }
                void shutDown(){
                    _client->shutDown();
                }
            private:
                Requestor::ptr _requestor;
                Dispatcher::ptr _dispatcher;
                TopicManager::ptr _manager;
                ClientBase::ptr _client;
        };
    }
}