#pragma once
#include "abstract.hpp"
#include "enum.hpp"
#include "Util.hpp"

namespace MyRpc
{
    using Address = std::pair<std::string,int>;
    class JsonMessage :public MessageBase
    {
        public:
            //length - mtype - idlength - id - body
            using ptr = std::shared_ptr<JsonMessage>;
            virtual std::string serialize()override
            {
                return JsonUtil::serialize(_body);     
            }
            virtual bool deserialize(const std::string& msg)
            {
                _body = JsonUtil::deserialize(msg);
                return true;
            }
        protected:
            Json::Value _body;
    };

    class JsonRequest :public JsonMessage
    {
        public:
            using ptr = std::shared_ptr<JsonRequest>;
            //check函数由具体的JsonRequest的派生类实现
    };

    class JsonResponse :public JsonMessage
    {
        public:
            using ptr = std::shared_ptr<JsonResponse>;
            virtual bool check() override
            {
                if(_body[KEY_RCODE].isNull())
                {
                    ELOG("缺失响应状态码!");
                    return false;
                }
                if(_body[KEY_RCODE].isIntegral() == false)
                {
                    ELOG("响应状态码类型错误!");
                    return false;
                }
                return true;
            }
            virtual Rcode rcode(){
                return static_cast<Rcode>(_body[KEY_RCODE].asInt());
            }
            virtual void setRcode(Rcode rcode)
            {
                _body[KEY_RCODE] = static_cast<int>(rcode); 
            }
    };

    class RpcRequest: public JsonRequest 
    {
        public:
            using ptr = std::shared_ptr<RpcRequest>;
            virtual bool check() override
            {
                if(_body[KEY_METHOD].isNull()){
                    ELOG("Rpc请求中方法缺失!");
                    return false;
                }
                if(_body[KEY_METHOD].isString()==false)
                {
                    ELOG("Rpc请求中方法类型错误!");
                    return false;
                }
                if(_body[KEY_PARAMS].isNull())
                {
                    ELOG("Rpc请求中参数缺失!");
                    return false;
                }
                if(_body[KEY_PARAMS].isObject()==false){
                    ELOG("Rpc请求中参数类型错误!");
                    return false;
                }
                return true;
            }
            std::string method()
            {
                return _body[KEY_METHOD].asString();
            }
            void setMethod(const std::string& method)
            {
                _body[KEY_METHOD] = method;
            }
            Json::Value parameters(){
                return _body[KEY_PARAMS];
            }
            void setParameters(const Json::Value& params)
            {
                _body[KEY_PARAMS] = params; 
            }
    };
    class TopicRequest :public JsonRequest
    {
        public:
            using ptr = std::shared_ptr<TopicRequest>;
            virtual bool check() override
            {
                if(_body[KEY_TOPIC_KEY].isNull()){
                    ELOG("主题请求中主题名缺失!");
                    return false;
                }
                if(_body[KEY_TOPIC_KEY].isString() == false){
                    ELOG("主题请求中主题名类型错误!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isNull()){
                    ELOG("主题请求中主题操作缺失!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isIntegral() == false){
                    ELOG("主题请求中主题操作类型错误!");
                    return false;
                }
                //只有主题发布具有MSG字段.服务器接收到消息后，会把消息转发给所有订阅了该主题的客户端
                if(_body[KEY_OPTYPE].asInt() == static_cast<int>(TopicOptype::TOPIC_PUBLISH) &&
                (_body[KEY_TOPIC_MSG].isNull() || _body[KEY_TOPIC_MSG].isString() ==false))
                { 
                    ELOG("主题请求中主题消息缺失或主题消息类型错误!");
                    return false;
                }
                return true;
                
            }
            std::string topicKey()
            {
                return _body[KEY_TOPIC_KEY].asString();
            }
            void setTopicKey(const std::string& key)
            {
                _body[KEY_TOPIC_KEY] = key;
            }
            TopicOptype topicOpType(){
                return static_cast<TopicOptype>(_body[KEY_OPTYPE].asInt());
            }
            void setOpType(TopicOptype opType){
                _body[KEY_OPTYPE] = static_cast<int>(opType);
            }
            std::string topicMsg(){
                return _body[KEY_TOPIC_MSG].asString();
            }
            void setTopicMsg(const std::string& msg)
            {
                _body[KEY_TOPIC_MSG] = msg;
            }

    };
    class ServiceRequest :public JsonRequest
    {
        public:
            using ptr = std::shared_ptr<ServiceRequest>;
            virtual bool check() override
            {
                if(_body[KEY_METHOD].isNull()){
                    ELOG("服务请求中方法名缺失!");
                    return false;
                }
                if(_body[KEY_METHOD].isString() == false){
                    ELOG("服务请求中方法名称类型错误!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isNull()){
                    ELOG("服务请求中方法操作缺失!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isIntegral() == false){
                    ELOG("服务请求中方法操作类型错误!");
                    return false;
                }
                //服务注册，上线，下线，都需要HOST 服务发现请求不需要
                if(_body[KEY_OPTYPE].asInt() != static_cast<int>(ServiceOptype::SERVICE_DISCOVERY) &&
                   _body[KEY_HOST].isNull() || _body[KEY_HOST].isObject() == false ||
                   _body[KEY_HOST][KEY_HOST_IP].isNull() || _body[KEY_HOST][KEY_HOST_IP].isString() == false||
                   _body[KEY_HOST][KEY_HOST_PORT].isNull() || _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false){
                    ELOG("服务请求中主机地址不存在或者主机类型错误!"); 
                    return false;
                }
                return true;
                
            }
            std::string method()
            {
                return _body[KEY_METHOD].asString();
            }
            void setMethod(const std::string& method)
            {
                _body[KEY_METHOD] = method;
            }
            ServiceOptype serviceOpType(){
                return static_cast<ServiceOptype>(_body[KEY_OPTYPE].asInt());
            }
            void setServiceOpType(ServiceOptype opType){
                _body[KEY_OPTYPE] = static_cast<int>(opType);
            }
            Address host(){
                return Address(_body[KEY_HOST][KEY_HOST_IP].asString(), _body[KEY_HOST][KEY_HOST_PORT].asInt());
            }
            void setHost(const Address& addr)
            {
                Json::Value value;
                value[KEY_HOST_IP] = addr.first;
                value[KEY_HOST_PORT] = addr.second;
                _body[KEY_HOST] = value;
            }
    };

    class RpcResponse: public JsonResponse
    {
        public:
            using ptr = std::shared_ptr<RpcResponse>;
            virtual bool check()override
            {
                if(_body[KEY_RCODE].isNull())
                {
                    ELOG("Rpc响应中缺失响应状态码!");
                    return false;
                }
                if(_body[KEY_RCODE].isIntegral() == false)
                {
                    ELOG("Rpc响应中响应状态码类型错误!");
                    return false;
                }
                if(_body[KEY_RESULT].isNull()){
                    ELOG("Rpc响应中缺失结果!");
                    return false;

                }
                //规定Rpc响应中的结果都以对象存储,不够灵活,不考虑
                // if(_body[KEY_RESULT].isObject() == false){
                //     ELOG("Rpc响应中结果类型错误!");
                //     return false;
                // }
                return true;
            }
            
            Json::Value result(){
                return _body[KEY_RESULT];
            }
            void setResult(const Json::Value& value)
            {
                _body[KEY_RESULT] = value;
            }
    };
    class TopicResponse: public JsonResponse
    {
        public:
            using ptr = std::shared_ptr<TopicResponse>;
    };
    class ServiceResponse: public JsonResponse
    {
        public:
            using ptr = std::shared_ptr<ServiceResponse>;
            virtual bool check() override
            {
                if(_body[KEY_RCODE].isNull())
                {
                    ELOG("Rpc响应中缺失响应状态码!");
                    return false;
                }
                if(_body[KEY_RCODE].isIntegral() == false)
                {
                    ELOG("Rpc响应中响应状态码类型错误!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isNull()){
                    ELOG("服务响应中方法操作缺失!");
                    return false;
                }
                if(_body[KEY_OPTYPE].isIntegral() == false){
                    ELOG("服务响应中方法操作类型错误!");
                    return false;
                }
                if(_body[KEY_OPTYPE].asInt() == static_cast<int>(ServiceOptype::SERVICE_DISCOVERY) &&
                (_body[KEY_HOST].isNull() || _body[KEY_HOST].isArray() ==  false ||
                 _body[KEY_METHOD].isNull() || _body[KEY_METHOD].isString() == false ))
                {
                    ELOG("服务发现响应中 KEYHOST or KEY_METHOD 缺失或类型错误!");
                    return false;
                }
                return true;

            }
            TopicOptype serviceOpType(){
                return static_cast<TopicOptype>(_body[KEY_OPTYPE].asInt());
            }
            void setServiceOpType(ServiceOptype opType){
                _body[KEY_OPTYPE] = static_cast<int>(opType);
            }
           
            std::string method()
            {
                return _body[KEY_METHOD].asString();
            }
            void setMethod(const std::string& method)
            {
                _body[KEY_METHOD] = method;
            }
            void setHosts(const std::vector<Address>& addrs)
            {
                for(const auto& addr: addrs)
                {
                    Json::Value value;
                    value[KEY_HOST_IP] = addr.first;
                    //第一现场，调了一天，niubi
                    // value[KEY_HOST_IP] = addr.second;
                    value[KEY_HOST_PORT] = addr.second;
                    _body[KEY_HOST].append(value);
                }
            }
            std::vector<Address> Hosts() const
            {
                std::vector<Address> hosts;
                for(int i = 0;i<_body[KEY_HOST].size();++i)
                {
                    Address addr;
                    const Json::Value &host = _body[KEY_HOST];
                    // addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                    // addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                    addr.first = host[i][KEY_HOST_IP].asString();
                    addr.second = host[i][KEY_HOST_PORT].asInt();
                    hosts.push_back(std::move(addr));
                }
                return hosts;
            }
    };

    class MessageFactory
    {
        public:
            static MessageBase::ptr create(Mtype mtype)
            {
                switch(mtype)
                {
                    case Mtype::REQ_RPC: return std::make_shared<RpcRequest>();
                    case Mtype::RSP_RPC: return std::make_shared<RpcResponse>();
                    case Mtype::REQ_TOPIC: return std::make_shared<TopicRequest>();
                    case Mtype::RSP_TOPIC: return std::make_shared<TopicResponse>();
                    case Mtype::REQ_SERVICE: return std::make_shared<ServiceRequest>();
                    case Mtype::RSP_SERVICE: return std::make_shared<ServiceResponse>();
                }
                return MessageBase::ptr();
            }
            template<typename T, typename ...Args>
            static std::shared_ptr<T> create(Args&& ...args)
            {
                return std::make_shared<T>(std::forward<Args>(args)...);
            }
    };
}