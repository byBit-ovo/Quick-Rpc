#pragma once
#include "requestor.hpp"
 
//客户端的服务发现方和服务注册方
namespace MyRpc{
    namespace Client{
        class Provider{
            public:
                Provider(const Requestor::ptr &requestor):_requestor(requestor){}
                using ptr = std::shared_ptr<Provider>;
                bool registerMethod(const ConnectionBase::ptr &conn,const std::string &method,const Address& host)
                {
                    auto msg = MessageFactory::create<ServiceRequest>();
                    msg->SetId(Uuid::uuid());
                    msg->SetType(Mtype::REQ_SERVICE);
                    msg->setServiceOpType(ServiceOptype::SERVICE_REGISTRY);
                    msg->setMethod(method);
                    msg->setHost(host);
                    MessageBase::ptr res;
                    // 同步发送请求，等待响应
                    bool ret = _requestor->send(conn,msg,res);
                    if(ret ==  false){
                        ELOG("%s 服务注册失败!",method.c_str());
                        return false;
                    }
                    auto rsp = std::dynamic_pointer_cast<ServiceResponse>(res);
                    if(rsp.get() == nullptr){
                        ELOG("类型转换失败!");
                        return false;
                    }
                    if(rsp->rcode() != Rcode::RCODE_OK){
                        ELOG("服务注册失败: %s", ErrReason(rsp->rcode()).c_str());
                        return false;
                    }
                    // DLOG("Service register successfully: %s= %s:%d",msg->method().c_str(),
                    return true;

                }
            private:
                Requestor::ptr _requestor;
        };

        class MethodHosts{
            public:
                using ptr = std::shared_ptr<MethodHosts>;
                MethodHosts():_index(0){}
                MethodHosts(const std::vector<Address>& hosts):
                _hosts(hosts.begin(),hosts.end()),_index(0){}
                //收到服务上线请求后调用
                void appendHost(const Address &host){
                    std::unique_lock<std::mutex> guard(_mutex);
                    _hosts.push_back(host);
                }
                void appendHosts(const std::vector<Address> &hosts){
                    std::unique_lock<std::mutex> guard(_mutex);
                    for(const Address &host: hosts){
                        _hosts.push_back(host);
                    }
                }
                //收到服务下线请求后调用
                void removeHost(const Address &host){
                    std::unique_lock<std::mutex> guard(_mutex);
                    for(auto iter = _hosts.begin();iter != _hosts.end();++iter)
                    {
                        if(*iter == host){
                            _hosts.erase(iter);
                            return;
                        }
                    }
                }
                Address chooseHost(){
                    if(isEmpty()){
                        return Address();
                    }
                    std::unique_lock<std::mutex> guard(_mutex);
                    Address host = _hosts[_index];
                    _index++;
                    _index %= _hosts.size();
                    return host;
                }
                bool isEmpty(){
                    std::unique_lock<std::mutex> guard(_mutex);
                    return _hosts.empty();
                }
            private:
                std::mutex _mutex;
                std::vector<Address> _hosts;
                size_t _index;
        };
        class Discoverer{
            public:
                using OfflineCallBack = std::function<void(const Address &)>;
                using ptr = std::shared_ptr<Discoverer>;
                Discoverer(const Requestor::ptr &req,const OfflineCallBack &off_call)
                :_requestor(req),_offline_call_back(off_call){}
                bool serviceDiscover(const ConnectionBase::ptr &conn, const std::string &method, Address &host){
                    {
                        std::unique_lock<std::mutex> guard(_mutex);
                        auto iter = _hosts.find(method);
                        if(iter != _hosts.end()){
                            if(iter->second->isEmpty() == false){
                                host = iter->second->chooseHost();
                                return true;
                            }
                        }
                    }
                    //如果找不到，则发起服务发现
                    auto req = MessageFactory::create<ServiceRequest>();
                    req->SetId(Uuid::uuid());
                    req->SetType(Mtype::REQ_SERVICE);
                    req->setServiceOpType(ServiceOptype::SERVICE_DISCOVERY);
                    req->setMethod(method);
                    MessageBase::ptr res;
                    bool ret = _requestor->send(conn,req,res);
                    if(ret== false){
                        ELOG("服务发现失败!");
                        return false;
                    }
                    ServiceResponse::ptr ser_res = std::dynamic_pointer_cast<ServiceResponse>(res);
                    if(!ser_res){
                        ELOG("类型转换失败!");
                        return false;
                    }
                    if(ser_res->rcode() != Rcode::RCODE_OK){
                        ELOG("服务发现失败!: %s", ErrReason(ser_res->rcode()).c_str());
                        return false;
                    }
                    if(ser_res->Hosts().empty()){
                        ELOG("尚未发现服务提供者!");
                        return false;
                    }
                    DLOG("Discover response: %s", ser_res->serialize().c_str());
                    std::unique_lock<std::mutex> guard(_mutex);
                    std::vector<Address> hosts = ser_res->Hosts();
                    _hosts[ser_res->method()] = std::make_shared<MethodHosts>(hosts);
                    host = _hosts[ser_res->method()]->chooseHost();
                    return true;
                }
                //该函数提供给 dispatcher 中的 服务上下线请求处理回调函数
                //服务提供者上下线后，该函数接收请求进行处理
                void onServiceRequest(const ConnectionBase::ptr &conn,const ServiceRequest::ptr &msg){
                    if(msg->check() == false){
                        return;
                    }
                    std::unique_lock<std::mutex> guard(_mutex);
                    //服务上线了
                    if(msg->serviceOpType() == ServiceOptype::SERVICE_ONLINE){
                        auto iter = _hosts.find(msg->method());
                        if(iter == _hosts.end()){
                            _hosts[msg->method()]  = std::make_shared<MethodHosts>();
                        }
                        _hosts[msg->method()]->appendHost(msg->host());
                        ILOG("A new service has been launch: %s",msg->method().c_str());
                    }//服务下线了
                    else if(msg->serviceOpType() == ServiceOptype::SERVICE_OFFLINE)
                    {
                        auto iter = _hosts.find(msg->method());
                        if(iter != _hosts.end()){
                            _hosts[msg->method()]->removeHost(msg->host());
                            if(_offline_call_back){
                                _offline_call_back(msg->host());
                            }
                            ILOG("A service has been offline: %s",msg->method().c_str());
                        }
                    }
                }
                void setOfflineCallBack(const OfflineCallBack &call){
                    _offline_call_back = call;
                }
            private:
                OfflineCallBack _offline_call_back;
                Requestor::ptr _requestor;
                std::unordered_map<std::string, MethodHosts::ptr> _hosts;
                std::mutex _mutex; 
        };
    }
}