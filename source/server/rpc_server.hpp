#pragma once
#include "../network/dispatcher.hpp"
#include "../client/rpc_client.hpp"
#include "rpc_router.hpp"
#include "service_manager.hpp"
#include "rpc_topic.hpp"

namespace MyRpc{
    namespace Server{
        //管理服务的注册与发现请求,注册中心
        class RegisterServer{
            public:
                using ptr = std::shared_ptr<RegisterServer>;
                RegisterServer(int port):
                _server(ServerFactory::create(port)),
                _dispatcher(std::make_shared<Dispatcher>()),
                _service_manager(std::make_shared<ServiceManager>()){
                    auto service_req = std::bind(&ServiceManager::onServiceRequest,_service_manager.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //注册 '服务发现与服务注册' 的回调函数
                    _dispatcher->registerHandler<ServiceRequest>(Mtype::REQ_SERVICE,service_req);
                    auto message_call = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //MuduoServer的总接口
                    auto close_call_back = std::bind(&RegisterServer::onClose,this,std::placeholders::_1);
                    _server->SetMessageCallBack(message_call);
                    _server->SetCloseCallBack(close_call_back);
                }
                void start(){
                    
                    _server->start();
                }
            private:
                void onClose(const ConnectionBase::ptr &conn){
                    _service_manager->onShutDown(conn);
                }
                ServerBase::ptr _server;
                Dispatcher::ptr _dispatcher; 
                ServiceManager::ptr _service_manager;
        };
        //Rpc调用服务，同时可以选择是否包含客户端的服务注册功能
        class RpcServer{
            public:
                using ptr = std::shared_ptr<RpcServer>;
                RpcServer(const Address &this_host,bool enable_register = false,const Address &reg_host = Address()):
                _this_host(this_host),
                _enable_register(enable_register),
                _server(ServerFactory::create(this_host.second)),
                _dispatcher(std::make_shared<Dispatcher>()),
                _router(std::make_shared<RpcRouter>()){
                    auto rpc_req = std::bind(&RpcRouter::onRpcRequest,_router.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _dispatcher->registerHandler<RpcRequest>(Mtype::REQ_RPC,rpc_req);
                    auto message_call_back = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    _server->SetMessageCallBack(message_call_back);
                    if(_enable_register == true){
                        _register_client = std::make_shared<Client::RegisterClient>(reg_host.first,reg_host.second);
                        _register_client->connect();
                    }
                    
                }
                void registerService(const ServiceDesc::ptr& service){
                    _router->registerService(service);
                    if(_enable_register == true && _register_client.get() != nullptr){
                        _register_client->registerMethod(service->name(),_this_host);
                    }
                }
                void start(){
                    _server->start();
                }
            private:
                bool _enable_register;
                Address _this_host;                             //记录本主机公网ip与端口用于发起服务注册
                Client::RegisterClient::ptr _register_client;
                ServerBase::ptr _server;
                Dispatcher::ptr _dispatcher; 
                // netless
                RpcRouter::ptr _router;
        };
        //消息发布订阅服务
        class TopicServer{
            public:
                using ptr = std::shared_ptr<TopicServer>;
                TopicServer(int port):
                _server(ServerFactory::create(port)),
                _dispatcher(std::make_shared<Dispatcher>()),
                _manager(std::make_shared<TopicManager>()){
                    auto topic_req = std::bind(&TopicManager::onTopicRequest,_manager.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //注册 '服务发现与服务注册' 的回调函数
                    _dispatcher->registerHandler<TopicRequest>(Mtype::REQ_TOPIC,topic_req);
                    auto message_call = std::bind(&Dispatcher::messageCallBack,_dispatcher.get(),
                    std::placeholders::_1,std::placeholders::_2);
                    //MuduoServer的总接口
                    auto close_call_back = std::bind(&TopicManager::onShutDown,_manager.get(),std::placeholders::_1);
                    _server->SetMessageCallBack(message_call);
                    _server->SetCloseCallBack(close_call_back);
                }
                void start(){
                    _server->start();
                }
            private:
                ServerBase::ptr _server;
                Dispatcher::ptr _dispatcher; 
                TopicManager::ptr _manager;
        };

    }
}
