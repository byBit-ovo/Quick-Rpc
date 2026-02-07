#pragma once
#include "requestor.hpp"

namespace MyRpc
{
    namespace Client
    {
        class RpcCaller
        {
        public:
            using ptr = std::shared_ptr<RpcCaller>;
            using PbCallBack = std::function<void(const google::protobuf::Value&)>;

            bool call(const ConnectionBase::ptr& conn, const std::string& method,
                      const google::protobuf::Struct& parameters, google::protobuf::Value& result)
            {
                RpcRequest::ptr req = std::dynamic_pointer_cast<RpcRequest>(MessageFactory::create(Mtype::REQ_RPC));
                req->SetId(Uuid::uuid());
                req->SetType(Mtype::REQ_RPC);
                req->setMethod(method);
                req->set_parameters(parameters);
                MessageBase::ptr response;
                DLOG("准备调用_request->send");
                bool ret = _requestor->send(conn, req, response);
                if (ret == false)
                {
                    ELOG("调用同步Rpc请求失败!");
                    return false;
                }
                RpcResponse::ptr rpc_resp = std::dynamic_pointer_cast<RpcResponse>(response);
                if (!rpc_resp)
                {
                    ELOG("响应类型转换失败!");
                    return false;
                }
                if (rpc_resp->rcode() != Rcode::RCODE_OK)
                {
                    ELOG("%s: %s", rpc_resp->GetId().c_str(), RcodeDesc.at(static_cast<int>(rpc_resp->rcode())).c_str());
                    return false;
                }
                DLOG("响应正常接受");
                result.CopyFrom(rpc_resp->result());
                return true;
            }

            bool call(const ConnectionBase::ptr& conn, const std::string& method,
                      const google::protobuf::Struct& parameters, std::future<google::protobuf::Value>& result)
            {
                RpcRequest::ptr req = std::dynamic_pointer_cast<RpcRequest>(MessageFactory::create(Mtype::REQ_RPC));
                req->SetId(Uuid::uuid());
                req->SetType(Mtype::REQ_RPC);
                req->setMethod(method);
                req->set_parameters(parameters);
                std::shared_ptr<std::promise<google::protobuf::Value>> promise_value = std::make_shared<std::promise<google::protobuf::Value>>();
                result = promise_value->get_future();
                Requestor::ResponseCallBack func = std::bind(&RpcCaller::CallOnAsync, this, promise_value, std::placeholders::_1);
                bool ret = _requestor->send(conn, req, func);
                if (ret == false)
                {
                    return false;
                }
                return true;
            }

            bool call(const ConnectionBase::ptr& conn, const std::string& method,
                      const google::protobuf::Struct& parameters, const PbCallBack& call)
            {
                RpcRequest::ptr req = std::dynamic_pointer_cast<RpcRequest>(MessageFactory::create(Mtype::REQ_RPC));
                req->SetId(Uuid::uuid());
                req->SetType(Mtype::REQ_RPC);
                req->setMethod(method);
                req->set_parameters(parameters);
                Requestor::ResponseCallBack call_back = std::bind(&RpcCaller::CallOnMessage, this, call, std::placeholders::_1);
                _requestor->send(conn, req, call_back);
                return true;
            }

            RpcCaller(const Requestor::ptr& requestor) : _requestor(requestor) {}

        private:
            void CallOnMessage(const PbCallBack& pbCall, MessageBase::ptr& msg)
            {
                auto resp = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!resp)
                {
                    ELOG("响应类型转换失败!");
                    return;
                }
                if (resp->rcode() != Rcode::RCODE_OK)
                {
                    ELOG("%s: %s", resp->GetId().c_str(), RcodeDesc.at(static_cast<int>(resp->rcode())).c_str());
                    return;
                }
                pbCall(resp->result());
            }

            void CallOnAsync(std::shared_ptr<std::promise<google::protobuf::Value>> rsp, MessageBase::ptr& msg)
            {
                auto resp = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!resp)
                {
                    ELOG("响应类型转换失败!");
                    return;
                }
                if (resp->rcode() != Rcode::RCODE_OK)
                {
                    ELOG("%s: %s", resp->GetId().c_str(), RcodeDesc.at(static_cast<int>(resp->rcode())).c_str());
                    return;
                }
                rsp->set_value(resp->result());
            }

            Requestor::ptr _requestor;
        };
    }
}
