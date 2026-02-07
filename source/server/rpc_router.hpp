#pragma once
#include "../network/message.hpp"
#include "../network/network.hpp"
#include <google/protobuf/struct.pb.h>

namespace MyRpc
{
    namespace Server
    {
        enum class parameterType
        {
            BOOL = 0,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT
        };

        inline bool checkValueType(parameterType type, const google::protobuf::Value& val)
        {
            switch (val.kind_case())
            {
            case google::protobuf::Value::KIND_NOT_SET:
                return false;
            case google::protobuf::Value::kBoolValue:
                return type == parameterType::BOOL;
            case google::protobuf::Value::kNumberValue:
                return type == parameterType::NUMERIC || type == parameterType::INTEGRAL;
            case google::protobuf::Value::kStringValue:
                return type == parameterType::STRING;
            case google::protobuf::Value::kListValue:
                return type == parameterType::ARRAY;
            case google::protobuf::Value::kStructValue:
                return type == parameterType::OBJECT;
            default:
                return false;
            }
        }

        class ServiceDesc
        {
        public:
            using ptr = std::shared_ptr<ServiceDesc>;
            using Func_t = std::function<void(const google::protobuf::Struct&, google::protobuf::Value&)>;
            ServiceDesc(const Func_t& func, const std::string& name, const parameterType type,
                        const std::unordered_map<std::string, parameterType>& parameters)
                : _call(func), _name(name), _return_type(type), _parameters(parameters) {}

            std::string name() { return _name; }
            bool checkOutParameters(const google::protobuf::Struct& parameters)
            {
                const auto& fields = parameters.fields();
                if (fields.size() != _parameters.size())
                {
                    ELOG("Rpc请求中参数个数错误");
                    return false;
                }
                for (const auto& desc : _parameters)
                {
                    auto it = fields.find(desc.first);
                    if (it == fields.end())
                    {
                        ELOG("Rpc请求中参数错误");
                        return false;
                    }
                    if (!checkValueType(desc.second, it->second))
                    {
                        ELOG("Rpc请求中参数类型错误");
                        return false;
                    }
                }
                return true;
            }

            bool call(const google::protobuf::Struct& params, google::protobuf::Value& result)
            {
                _call(params, result);
                if (!checkValueType(_return_type, result))
                {
                    ELOG("Rpc响应结果类型错误");
                    return false;
                }
                return true;
            }

        private:
            Func_t _call;
            std::string _name;
            parameterType _return_type;
            std::unordered_map<std::string, parameterType> _parameters;
        };

        class ServiceDescBuilder
        {
        public:
            using Self = ServiceDescBuilder;
            using ptr = std::shared_ptr<Self>;

            Self& setMethodName(const std::string& name)
            {
                _name = name;
                return *this;
            }
            Self& setReturnType(parameterType vtype)
            {
                _return_type = vtype;
                return *this;
            }
            Self& setParamsDesc(const std::string& pname, parameterType type)
            {
                _parameters.insert(std::make_pair(pname, type));
                return *this;
            }
            Self& setCallback(const ServiceDesc::Func_t& cb)
            {
                _call = cb;
                return *this;
            }
            ServiceDesc::ptr build()
            {
                return std::make_shared<ServiceDesc>(_call, _name, _return_type, _parameters);
            }

        private:
            ServiceDesc::Func_t _call;
            std::string _name;
            parameterType _return_type;
            std::unordered_map<std::string, parameterType> _parameters;
        };

        class RpcServiceManager
        {
        public:
            using ptr = std::shared_ptr<RpcServiceManager>;
            void insert(const ServiceDesc::ptr& desc)
            {
                std::lock_guard<std::mutex> guard(_lock);
                _services.insert(std::make_pair(desc->name(), desc));
            }
            void remove(const std::string& name)
            {
                std::lock_guard<std::mutex> guard(_lock);
                _services.erase(name);
            }
            ServiceDesc::ptr search(const std::string& name)
            {
                std::lock_guard<std::mutex> guard(_lock);
                auto iter = _services.find(name);
                if (iter != _services.end())
                {
                    return iter->second;
                }
                return ServiceDesc::ptr();
            }

        private:
            std::unordered_map<std::string, ServiceDesc::ptr> _services;
            std::mutex _lock;
        };

        class RpcRouter
        {
        public:
            using ptr = std::shared_ptr<RpcRouter>;
            RpcRouter() : _manager(std::make_shared<RpcServiceManager>()) {}

            void registerService(const ServiceDesc::ptr& service)
            {
                _manager->insert(service);
            }

            void onRpcRequest(const ConnectionBase::ptr& conn, const RpcRequest::ptr& msg)
            {
                std::string name = msg->method();
                ServiceDesc::ptr service = _manager->search(name);
                if (service.get() == nullptr)
                {
                    ELOG("Rpc请求方法不存在!");
                    return response(conn, google::protobuf::Value(), Rcode::RCODE_NOT_FOUND_SERVICE, msg->GetId());
                }
                if (!service->checkOutParameters(msg->parameters()))
                {
                    return response(conn, google::protobuf::Value(), Rcode::RCODE_INVALID_PARAMS, msg->GetId());
                }
                google::protobuf::Value result;
                if (!service->call(msg->parameters(), result))
                {
                    return response(conn, google::protobuf::Value(), Rcode::RCODE_INVALID_RESULT, msg->GetId());
                }
                return response(conn, result, Rcode::RCODE_OK, msg->GetId());
            }

        private:
            void response(const ConnectionBase::ptr& conn, const google::protobuf::Value& res, Rcode rcode, const std::string& id)
            {
                RpcResponse::ptr respon = MessageFactory::create<RpcResponse>();
                respon->setRcode(rcode);
                respon->set_result(res);
                respon->SetType(MyRpc::Mtype::RSP_RPC);
                respon->SetId(id);
                conn->send(respon);
            }
            RpcServiceManager::ptr _manager;
        };
    }
}
