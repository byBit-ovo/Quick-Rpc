#pragma once
#include "../network/dispatcher.hpp"
#include "rpc_caller.hpp"
#include "../server/rpc_topic.hpp"      
#include "../network/message.hpp"
#include "../network/network.hpp"

namespace MyRpc{
    namespace Client{
        //for sender client or subscriber client 
        class TopicManager{
            public:
                using ptr = std::shared_ptr<TopicManager>;
                using Msg_Func_t = std::function<void(const std::string &topic_name,const std::string &msg)>;
                TopicManager(Requestor::ptr requestor):_requestor(requestor){}
                //only send and recieve msg, create/remove/subscribe above client
                bool createTopic(const ConnectionBase::ptr &conn,const std::string &name){
                    return send(conn,name,TopicOptype::TOPIC_CREATE);
                }
                bool removeTopic(const ConnectionBase::ptr &conn,const std::string &name){

                    return send(conn,name,TopicOptype::TOPIC_REMOVE);
                }
                bool cancelTopic(const ConnectionBase::ptr &conn,const std::string &name){
                    {
                        std::unique_lock<std::mutex> guard(_lock);
                        _publish_router.erase(name);
                    }
                    return send(conn,name,TopicOptype::TOPIC_CANCEL);
                }

                bool publish(const ConnectionBase::ptr &conn,const std::string &topic_name,const std::string &msg){
                    return send(conn,topic_name,TopicOptype::TOPIC_PUBLISH,msg);
                }
                //subscribe topic and regsiter handler to handle it 
                bool subscribeTopic(const ConnectionBase::ptr &conn,const std::string &name,const Msg_Func_t &handler){ 
                    //prevent from recieving publish_msg before handler is registered
                    {
                        std::unique_lock<std::mutex> guard(_lock);
                        _publish_router.insert(std::make_pair(name,handler));
                    }
                    bool ret = send(conn,name,TopicOptype::TOPIC_SUBSCRIBE);
                    if(ret == false){
                        //if subscribe false, remove handler
                        std::unique_lock<std::mutex> guard(_lock);
                        _publish_router.erase(name);
                    }
                    return ret;
                }

                void onPublish(const ConnectionBase::ptr &conn, const TopicRequest::ptr &msg){
                    if(msg->topicOpType() != TopicOptype::TOPIC_PUBLISH){
                        ELOG("recieve wrong message on topic: %s",msg->topicKey().c_str());
                        return;
                    }
                    auto iter = _publish_router.find(msg->topicKey());
                    if(iter == _publish_router.end() || !(iter->second)){
                        ELOG("Missing handler for msg...");
                        return;
                    }
                    iter->second(msg->topicKey(),msg->topicMsg()); 
                } 

            private:
                //send and recieve
                bool send(const ConnectionBase::ptr &conn,const std::string &name,TopicOptype optype,
                    const std::string &msg= ""){
                    TopicRequest::ptr topic_req = MessageFactory::create<TopicRequest>();
                    topic_req->SetId(Uuid::uuid());
                    topic_req->SetType(Mtype::REQ_TOPIC);
                    topic_req->setOpType(optype);
                    topic_req->setTopicKey(name);
                    if(optype == TopicOptype::TOPIC_PUBLISH){
                        topic_req->setTopicMsg(msg); 
                    }
                    MessageBase::ptr rsp;
                    bool ret = _requestor->send(conn,topic_req,rsp);
                    if(ret==false){
                        ELOG("Topic operator error!");
                        return false;
                    }
                    TopicResponse::ptr topic_resp = std::dynamic_pointer_cast<TopicResponse>(rsp);
                    if(!topic_resp){
                        ELOG("Topic response format-invert fail!");
                        return false;
                    }
                    if(topic_resp->rcode() != Rcode::RCODE_OK){
                        ELOG("Topic response error: %s: %s",topic_resp->GetId().c_str(),RcodeDesc.at(static_cast<int>(topic_resp->rcode())).c_str());
                        return false;
                    }
                    return true;
                }
                Requestor::ptr _requestor;
                std::mutex _lock;
                std::unordered_map<std::string,Msg_Func_t> _publish_router;
        };
    }
}