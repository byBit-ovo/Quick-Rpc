#pragma once
#include "../network/network.hpp"
#include "../network/pb_message.hpp"
#include "../network/dispatcher.hpp"
#include <future>
namespace MyRpc
{
    namespace Client
    {
        //多线程环境中，为了明确客户端接受到的响应对应于哪一条请求，Requestor用于保存请求与响应之间的映射关系
        //此模块是针对所有Request设计的，因此对响应的处理方法不能写成固定的Json::Value(result),而是MessageBase
        //服务器客户端通用模块
        class Requestor
        {
            public:
                using ptr = std::shared_ptr<Requestor>;
                using ResponseCallBack = std::function<void(MessageBase::ptr&)>;
                //将发出的请求保管起来，等到拿到对应的响应，再返回给上层,否则无法确定收到的响应对应哪一条请求
                struct RequestDesc{
                    using ptr = std::shared_ptr<RequestDesc>;
                    ReqType _type; 
                    MessageBase::ptr _req;
                    //如果是异步请求，将对应的响应放入到promise
                    //这里不单是Rpc响应，还有ServiceResponse,TopicResponse,
                    //因此不能只存std::promise<Json::Value>
                    std::promise<MessageBase::ptr> _response;
                    //如果是同步请求，调用该函数
                    ResponseCallBack _call_back;
                };
                //该函数注册给dispatcher, 收到response时，,将response设置进对应的RequestDesc或调用回调函数
                //dispatcher 中的 Mtype::RpcResponse:  onResponse,针对所有的Response
                void onResponse(const ConnectionBase::ptr &conn, MessageBase::ptr& msg){
                    auto desc = find(msg->GetId());
                    if(desc.get() == nullptr){
                        ELOG("收到的响应未找到请求描述!请求id: %s", msg->GetId().c_str());
                        return;
                    }
                    DLOG("收到的响应找到匹配的请求描述");
                    if(desc->_type == ReqType::REQ_ASYNC){
                        //收到Response时，将结果放入promise,供上层获取
                        desc->_response.set_value(msg);
                    }
                    else if(desc->_type == ReqType::REQ_CALLBACK){
                        //收到Response时，调用上层设置好的回调函数
                        if(desc->_call_back)
                            desc->_call_back(msg);
                    }
                    else{
                        ELOG("收到了非Async,非回调请求对应的响应");
                    }
                    //如果在 std::promise 被析构之前，已经通过 set_value() 或 set_exception() 设置了结果，
                    //那么关联的 std::future 可以安全地调用 get() 来获取该结果或异常。
                    removeDesc(msg->GetId());
                
                }
                //收到响应调用回调处理
                bool send(const ConnectionBase::ptr& conn, const MessageBase::ptr& msg,const ResponseCallBack& call)
                {
                    RequestDesc::ptr desc = insertDesc(msg,ReqType::REQ_CALLBACK,call);
                    if(desc.get()==nullptr){
                        ELOG("构造请求描述失败!");
                        return false;
                    }
                    conn->send(msg);
                    return true;
                }
                //异步
                //上层在发送请求时传入future,将其关联到ReqDesc中的promise,onResponse后，会将收到的响应设置进future,外部通过get获取响应
                bool send(const ConnectionBase::ptr& conn, const MessageBase::ptr& msg,std::future<MessageBase::ptr>& resp)
                {
                    // DLOG("进入异步requestor: send");
                    RequestDesc::ptr desc = insertDesc(msg,ReqType::REQ_ASYNC);
                    if(desc.get()==nullptr){
                        ELOG("构造请求描述失败!");
                        return false;
                    }
                    // DLOG("准备调用send");
                    conn->send(msg);
                    // DLOG("准备get_futurte");
                    resp = desc->_response.get_future();
                    // DLOG("离开异步requestor: send");
                    return true;
                }
                //同步获取响应
                bool send(const ConnectionBase::ptr& conn, const MessageBase::ptr& msg,MessageBase::ptr& resp){
                    // DLOG("进入同步requestor: send");
                    std::future<MessageBase::ptr> resp_future;
                    bool ret = send(conn,msg,resp_future);
                    if(ret == false){
                        return false;
                    }
                    //立即get达到同步的效果
                    resp = resp_future.get();
                    // DLOG("离开同步requestor: send");
                    return true;
                }
            private:
                //添加请求描述
                RequestDesc::ptr insertDesc(const MessageBase::ptr& req, ReqType type,
                    const ResponseCallBack& call=ResponseCallBack()){
                    std::unique_lock<std::mutex> guard(_lock);
                    RequestDesc::ptr desc = std::make_shared<RequestDesc>();
                    desc->_type = type;
                    desc->_req = req;
                    if(type == ReqType::REQ_CALLBACK && call){
                        desc->_call_back = call;
                    }
                    _requests_desc.insert(std::make_pair(req->GetId(),desc));
                    return desc;
                }
                //移除请求描述
                void removeDesc(const std::string& id){
                    std::unique_lock<std::mutex> guard(_lock);
                    _requests_desc.erase(id);
                }
                //查找请求描述
                RequestDesc::ptr find(const std::string& id){
                    // 这把锁没加
                    std::unique_lock<std::mutex> guard(_lock);
                    auto iter = _requests_desc.find(id);
                    if(iter==_requests_desc.end()){
                        return RequestDesc::ptr();
                    }
                    return iter->second;
                }
                std::mutex _lock;
                //                      uuid(): RequestDesc
                std::unordered_map<std::string, RequestDesc::ptr> _requests_desc;
        };
    }
}