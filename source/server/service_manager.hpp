#pragma once
#include "../network/network.hpp"
#include "../network/pb_message.hpp"
#include <set>
namespace MyRpc
{
    //该模块针对这些请求
    // enum class ServiceOptype
    // {
    //     SERVICE_REGISTRY = 0,
    //     SERVICE_DISCOVERY,
    //     SERVICE_ONLINE,
    //     SERVICE_OFFLINE,
    //     SERVICE_UNKNOW
    // };
    //整体采用 观察者模式
    namespace Server
    {
        //服务提供者管理
        class ProviderManager{
            public:
                using ptr = std::shared_ptr<ProviderManager>;
                struct Provider{
                    using ptr = std::shared_ptr<Provider>;
                    Address host;
                    std::vector<std::string> methods;
                    std::mutex mutex;
                    ConnectionBase::ptr conn;
                    void appendMethod(const std::string& method){
                        std::unique_lock<std::mutex> guard(mutex);
                        methods.emplace_back(method);
                    }
                    Provider(const Address &host_, const ConnectionBase::ptr &conn_):
                            host(host_),conn(conn_){}
                };
                //当有服务提供者提供服务时，将其插入
                Provider::ptr addProvider(const Address &host, const ConnectionBase::ptr &conn,const std::string &method){
                    std::unordered_map<ConnectionBase::ptr, Provider::ptr>::iterator iter;
                    {
                        std::unique_lock<std::mutex> guard(_mutex);
                        iter = _connsToProvider.find(conn);
                        if(iter == _connsToProvider.end()){
                            iter = _connsToProvider.insert({conn,std::make_shared<Provider>(host,conn)}).first;
                        }
                        //该服务新增服务提供者
                        _providers[method].insert(iter->second);
                    }
                    auto provider = iter->second;
                    //该提供者新增服务
                    provider->appendMethod(method);
                    return provider;
                }
                //当有一个服务提供者断开连接时，找到Provider,下线他的所有服务,或已经存在的服务者上线新服务时
                Provider::ptr findProvider(const ConnectionBase::ptr &conn){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto iter = _connsToProvider.find(conn);
                    if(iter == _connsToProvider.end()){
                        return Provider::ptr();
                    }
                    return iter->second;
                }
                //当一个服务提供者断开连接时，删除他的关联信息
                bool removeProvider(const ConnectionBase::ptr &conn){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto iter = _connsToProvider.find(conn);
                    if(iter == _connsToProvider.end()){
                        return false;
                    }
                    //移除该服务提供者所有服务
                    for(auto &method: iter->second->methods){
                        auto it = _providers.find(method);
                        if(it == _providers.end()){
                            continue;
                        }
                        std::set<Provider::ptr> &these_methods = it->second;
                        these_methods.erase(iter->second);
                    }
                    _connsToProvider.erase(conn);
                    return true;
                }
                std::vector<Address> getHosts(const std::string &method)
                {
                    auto iter = _providers.find(method);
                    std::vector<Address> result;
                    if(iter != _providers.end())
                    {
                        for(const Provider::ptr &provider: iter->second){
                            result.push_back(provider->host);
                        }
                    }
                    return result;
                }
            private:    
                std::mutex _mutex;
                //某一个服务的所有提供者
                std::unordered_map<std::string,std::set<Provider::ptr>> _providers;
                //每个连接与服务者的关系
                std::unordered_map<ConnectionBase::ptr, Provider::ptr> _connsToProvider;
        };

        class DiscoverManager{
            public:
                using ptr = std::shared_ptr<DiscoverManager>;
                //记录发现者发现过的服务
                struct Discoverer{
                    using ptr = std::shared_ptr<Discoverer>;
                    std::mutex mutex;
                    ConnectionBase::ptr conn;
                    //该发现者发现过的服务
                    std::vector<std::string> methods;
                    Discoverer(const ConnectionBase::ptr co):conn(co){}
                    void appendMethod(const std::string &method){
                        std::unique_lock<std::mutex> guard(mutex);
                        methods.push_back(method);
                    }
                };
                //新增发现者
                Discoverer::ptr addDiscoverer(const ConnectionBase::ptr &conn,const std::string &method){
                    Discoverer::ptr discoverer;
                    {
                        std::unique_lock<std::mutex> guard(_mutex);
                        auto iter = _conns.find(conn);
                        if(iter == _conns.end()){
                            discoverer = std::make_shared<Discoverer>(conn);
                            _conns.insert(std::make_pair(conn,discoverer));
                        }
                        else{
                            discoverer = iter->second;
                        }
                        _discoverers[method].insert(discoverer);
                    }
                    discoverer->appendMethod(method);
                    return discoverer;
                }
                //客户端断开连接，删除关联数据
                bool removeDiscover(const ConnectionBase::ptr &conn){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto iter = _conns.find(conn);
                    if(iter == _conns.end()){
                        return false;
                    }
                    //移除该发现者发现过的服务记录
                    for(const std::string &method: iter->second->methods)
                    {
                        if(_discoverers.count(method) != 0){
                            _discoverers[method].erase(iter->second);
                        }
                    }
                    _conns.erase(iter); 
                    return true;
                }
                //新的服务提供者上线后，通知发现了该服务的客户端
                void onlineNotify(const std::string& method, const Address& host){
                    return notify(method,host,ServiceOptype::SERVICE_ONLINE);
                }
                //服务提供者下线后，通知发现过该服务的客户端
                void offlineNotify(const std::string& method, const Address& host){         
                    return notify(method,host,ServiceOptype::SERVICE_OFFLINE);
                }
            private:
                void notify(const std::string& method, const Address& host,ServiceOptype optype){
                    std::unique_lock<std::mutex> guard(_mutex);
                    auto iter = _discoverers.find(method);
                    if(iter == _discoverers.end()){
                        return;
                    }
                    ServiceRequest::ptr req = MessageFactory::create<ServiceRequest>();
                    req->SetId(Uuid::uuid());
                    req->setServiceOpType(optype);
                    req->setMethod(method);
                    req->SetType(Mtype::REQ_SERVICE); 
                    req->setHost(host);
                    for(auto &discoverer : iter->second){
                        discoverer->conn->send(req);
                    }               
                }
                std::mutex _mutex;
                //记录某个服务被哪些发现者发现过
                std::unordered_map<std::string,std::set<Discoverer::ptr>> _discoverers;
                //记录连接与发现者的关系
                std::unordered_map<ConnectionBase::ptr,Discoverer::ptr> _conns;
        };

        class ServiceManager{
            public:
                using ptr = std::shared_ptr<ServiceManager>;
                ServiceManager():
                _providers(std::make_shared<ProviderManager>()),
                _discoverers(std::make_shared<DiscoverManager>()){}
                //服务注册/服务发现,该函数注册到dispatcher中的ServiceReq: OnServiceRequest
                void onServiceRequest(const ConnectionBase::ptr &conn,const ServiceRequest::ptr &msg)
                {
                    ServiceOptype optype = msg->serviceOpType();
                    if(optype == ServiceOptype::SERVICE_REGISTRY){
                        DLOG("recieve a service-register request");
                        DLOG("Method: %s,host:%s: %d",msg->method().c_str(),msg->host().first.c_str(),msg->host().second);
                        _providers->addProvider(msg->host(),conn,msg->method());
                        _discoverers->onlineNotify(msg->method(),msg->host());
                        return responseRegister(conn,msg);
                    }
                    else if(optype == ServiceOptype::SERVICE_DISCOVERY){
                        DLOG("recieve a service-discover request");
                        DLOG("Method: %s",msg->method().c_str());
                        _discoverers->addDiscoverer(conn,msg->method());
                        return responseDiscover(conn,msg);
                    }
                    else{
                        ELOG("ServiceMsg Type Error!");
                        return responseErr(conn,msg);
                    }
                }

                // 若提供者连接断开，则通知发现者该服务下线
                void onShutDown(const ConnectionBase::ptr &conn){
                    auto provider = _providers->findProvider(conn);
                    if(provider.get() != nullptr){
                        //服务提供者断开连接，首先给其服务发现者发送下线通知
                        DLOG("Provider off-line,remove it from service_manager");

                        for(auto &method: provider->methods){
                            _discoverers->offlineNotify(method,provider->host);
                        }
                        _providers->removeProvider(conn);
                    }
                    //若该连接不是发现者，内部则忽略
                    _discoverers->removeDiscover(conn);
                }
            private:
                void responseRegister(const ConnectionBase::ptr &conn, const ServiceRequest::ptr &req){
                    auto res = MessageFactory::create<ServiceResponse>();
                    res->SetId(req->GetId());
                    res->SetType(Mtype::RSP_SERVICE);
                    res->setRcode(Rcode::RCODE_OK);
                    res->setServiceOpType(ServiceOptype::SERVICE_REGISTRY);
                    conn->send(res);
                }
                void responseDiscover(const ConnectionBase::ptr &conn, const ServiceRequest::ptr &req)
                {
                    auto res = MessageFactory::create<ServiceResponse>();
                    std::string method = req->method();
                    res->setMethod(method);
                    res->SetId(req->GetId());
                    res->SetType(Mtype::RSP_SERVICE);
                    res->setRcode(Rcode::RCODE_OK);
                    res->setServiceOpType(ServiceOptype::SERVICE_DISCOVERY);
                    auto hosts = _providers->getHosts(method);
                    if(hosts.empty()){
                        res->setRcode(Rcode::RCODE_NOT_FOUND_SERVICE);
                    }else{
                        res->setHosts(hosts);
                        // DLOG("Response to discover: %s",res->serialize().c_str());
                    }
                    return conn->send(res);
                }
                void responseErr(const ConnectionBase::ptr &conn, const ServiceRequest::ptr &req){
                    auto res = MessageFactory::create<ServiceResponse>();
                    res->SetId(req->GetId());
                    res->SetType(Mtype::RSP_SERVICE);
                    res->setRcode(Rcode::RCODE_INVALID_OPTYPE);
                    res->setServiceOpType(ServiceOptype::SERVICE_UNKNOW_PB);
                    conn->send(res);
                }
                ProviderManager::ptr _providers;
                DiscoverManager::ptr _discoverers;
        };
    }
}