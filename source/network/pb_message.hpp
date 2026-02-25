#pragma once
#include "abstract.hpp"
#include "enum.hpp"
#include "Util.hpp"
#include "pb/message.pb.h"

namespace MyRpc
{
    using Address = std::pair<std::string, int>;

    // Pb 消息基类 - 持有 proto message，实现 serialize/deserialize
    class PbMessageBase : public MessageBase
    {
    public:
        virtual std::string serialize() override { return _body->SerializeAsString(); }
        virtual bool deserialize(const std::string& msg) override { return _body->ParseFromString(msg); }

    protected:
        google::protobuf::Message* _body = nullptr;
    };

    class PbRequest : public PbMessageBase
    {
    public:
        using ptr = std::shared_ptr<PbRequest>;
    };

    class PbResponse : public PbMessageBase
    {
    public:
        using ptr = std::shared_ptr<PbResponse>;
        virtual bool check() override { return _body != nullptr; }
        virtual Rcode rcode() = 0;
        virtual void setRcode(Rcode rcode) = 0;
    };

    class RpcRequest : public PbRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;
        RpcRequest() { _body = &_rpc_req; }
        virtual bool check() override
        {
            if (_rpc_req.method().empty())
            {
                ELOG("Rpc请求中方法缺失!");
                return false;
            }
            if (!_rpc_req.has_parameters())
            {
                ELOG("Rpc请求中参数缺失!");
                return false;
            }
            return true;
        }
        std::string method() { return _rpc_req.method(); }
        void setMethod(const std::string& method) { _rpc_req.set_method(method); }
        google::protobuf::Struct* mutable_parameters() { return _rpc_req.mutable_parameters(); }
        const google::protobuf::Struct& parameters() const { return _rpc_req.parameters(); }
        void set_parameters(const google::protobuf::Struct& params) { *_rpc_req.mutable_parameters() = params; }

    private:
        MyRpc::RpcRequestPb _rpc_req;
    };

    class RpcResponse : public PbResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;
        RpcResponse() { _body = &_rpc_rsp; }
        virtual bool check() override
        {
            if (!_body)
                return false;
            return true;
        }
        virtual Rcode rcode() { return static_cast<Rcode>(_rpc_rsp.rcode()); }
        virtual void setRcode(Rcode rcode) { _rpc_rsp.set_rcode(static_cast<int>(rcode)); }
        google::protobuf::Struct* mutable_result() { return _rpc_rsp.mutable_result(); }
        const google::protobuf::Struct& result() const { return _rpc_rsp.result(); }
        void set_result(const google::protobuf::Struct& val) { *_rpc_rsp.mutable_result() = val; }

    private:
        MyRpc::RpcResponsePb _rpc_rsp;
    };

    class TopicRequest : public PbRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;
        TopicRequest() { _body = &_topic_req; }
        virtual bool check() override
        {
            if (_topic_req.topic_key().empty())
            {
                ELOG("主题请求中主题名缺失!");
                return false;
            }
            if (_topic_req.optype() == static_cast<int>(TopicOptype::TOPIC_PUBLISH) && _topic_req.topic_msg().empty())
            {
                ELOG("主题请求中主题消息缺失!");
                return false;
            }
            return true;
        }
        std::string topicKey() { return _topic_req.topic_key(); }
        void setTopicKey(const std::string& key) { _topic_req.set_topic_key(key); }
        TopicOptype topicOpType() { return static_cast<TopicOptype>(_topic_req.optype()); }
        void setOpType(TopicOptype opType) { _topic_req.set_optype(static_cast<int>(opType)); }
        std::string topicMsg() { return _topic_req.topic_msg(); }
        void setTopicMsg(const std::string& msg) { _topic_req.set_topic_msg(msg); }

    private:
        MyRpc::TopicRequestPb _topic_req;
    };

    class TopicResponse : public PbResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;
        TopicResponse() { _body = &_topic_rsp; }
        virtual Rcode rcode() { return static_cast<Rcode>(_topic_rsp.rcode()); }
        virtual void setRcode(Rcode rcode) { _topic_rsp.set_rcode(static_cast<int>(rcode)); }

    private:
        MyRpc::TopicResponsePb _topic_rsp;
    };

    class ServiceRequest : public PbRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;
        ServiceRequest() { _body = &_svc_req; }
        virtual bool check() override
        {
            if (_svc_req.method().empty())
            {
                ELOG("服务请求中方法名缺失!");
                return false;
            }
            if (_svc_req.optype() != static_cast<int>(ServiceOptype::SERVICE_DISCOVERY))
            {
                if (!_svc_req.has_host())
                {
                    ELOG("服务请求中主机地址不存在!");
                    return false;
                }
            }
            return true;
        }
        std::string method() { return _svc_req.method(); }
        void setMethod(const std::string& method) { _svc_req.set_method(method); }
        ServiceOptype serviceOpType() { return static_cast<ServiceOptype>(_svc_req.optype()); }
        void setServiceOpType(ServiceOptype opType) { _svc_req.set_optype(static_cast<int>(opType)); }
        Address host()
        {
            return Address(_svc_req.host().ip(), _svc_req.host().port());
        }
        void setHost(const Address& addr)
        {
            _svc_req.mutable_host()->set_ip(addr.first);
            _svc_req.mutable_host()->set_port(addr.second);
        }

    private:
        MyRpc::ServiceRequestPb _svc_req;
    };

    class ServiceResponse : public PbResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;
        ServiceResponse() { _body = &_svc_rsp; }
        virtual bool check() override { return _body != nullptr; }
        virtual Rcode rcode() { return static_cast<Rcode>(_svc_rsp.rcode()); }
        virtual void setRcode(Rcode rcode) { _svc_rsp.set_rcode(static_cast<int>(rcode)); }
        ServiceOptype serviceOpType() { return static_cast<ServiceOptype>(_svc_rsp.optype()); }
        void setServiceOpType(ServiceOptype opType) { _svc_rsp.set_optype(static_cast<int>(opType)); }
        std::string method() { return _svc_rsp.method(); }
        void setMethod(const std::string& method) { _svc_rsp.set_method(method); }
        void setHosts(const std::vector<Address>& addrs)
        {
            _svc_rsp.clear_hosts();
            for (const auto& addr : addrs)
            {
                auto* h = _svc_rsp.add_hosts();
                h->set_ip(addr.first);
                h->set_port(addr.second);
            }
        }
        std::vector<Address> Hosts() const
        {
            std::vector<Address> hosts;
            for (int i = 0; i < _svc_rsp.hosts_size(); ++i)
            {
                const auto& h = _svc_rsp.hosts(i);
                hosts.push_back({h.ip(), h.port()});
            }
            return hosts;
        }

    private:
        MyRpc::ServiceResponsePb _svc_rsp;
    };

    class MessageFactory
    {
    public:
        static MessageBase::ptr create(Mtype mtype)
        {
            switch (mtype)
            {
            case Mtype::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case Mtype::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case Mtype::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case Mtype::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case Mtype::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case Mtype::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
            }
            return MessageBase::ptr();
        }
        template <typename T, typename... Args>
        static std::shared_ptr<T> create(Args&&... args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
        
    };
}
