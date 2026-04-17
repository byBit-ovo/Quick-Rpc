#pragma once
#include "../network/network.hpp"
#include "../network/pb_message.hpp"
#include <set>
#include <unordered_set>
namespace MyRpc
{
    namespace Server
    {
        class TopicManager{
            public:
                using ptr = std::shared_ptr<TopicManager>;
                //To dispatcher
                void onTopicRequest(const ConnectionBase::ptr &conn, const TopicRequest::ptr &msg){
                    //create、delete、subscribe、cancel、publish
                    TopicOptype topic_optype = msg->topicOpType();
                    bool ret = true;
                    switch(topic_optype){
                        case TopicOptype::TOPIC_CREATE: createTopic(conn, msg); break;
                        case TopicOptype::TOPIC_REMOVE: removeTopic(conn, msg); break;
                        case TopicOptype::TOPIC_SUBSCRIBE: ret = subscribeTopic(conn, msg); break;
                        case TopicOptype::TOPIC_CANCEL: cancelTopic(conn, msg); break;
                        case TopicOptype::TOPIC_PUBLISH: ret = publishMsg(conn, msg); break;
                        default:  return responseErr(conn, msg);
                    }
                    if (ret == false) return responseErr(conn, msg);
                    return responseNormal(conn, msg);
                }
                
                void onShutDown(const ConnectionBase::ptr &conn){
                    //only handle when this conn is a subscriber
                    std::vector<Topic::ptr> topics;
                    Subscriber::ptr subscriber;
                    {
                        std::lock_guard<std::mutex> guard(_mutex);
                        auto iter = _subscribers.find(conn);
                        if(iter == _subscribers.end()){
                            return;
                        }
                        subscriber = iter->second;
                        for(const std::string &topic: subscriber->topics){
                            auto iter_topic = _topics.find(topic);
                            if(iter_topic == _topics.end()){
                                continue;
                            }
                            //find topics subscribed by this person
                            topics.push_back(iter_topic->second);
                        }
                        _subscribers.erase(conn);
                    }
                    for(auto &topic: topics){
                        topic->removeSubscriber(subscriber);
                    }
                }
            private:
                void responseNormal(const ConnectionBase::ptr &conn, const TopicRequest::ptr &req){
                    auto res = MessageFactory::create<TopicResponse>();
                    res->SetId(req->GetId());
                    res->SetType(Mtype::RSP_TOPIC);
                    res->setRcode(Rcode::RCODE_OK);
                    conn->send(res);
                }
                void responseErr(const ConnectionBase::ptr &conn, const TopicRequest::ptr &req){
                    auto res = MessageFactory::create<TopicResponse>();
                    res->SetId(req->GetId());
                    res->SetType(Mtype::RSP_TOPIC);
                    res->setRcode(Rcode::RCODE_INVALID_OPTYPE);
                    conn->send(res);
                }
                void responseNoTopic(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    auto res = MessageFactory::create<TopicResponse>();
                    res->setRcode(Rcode::RCODE_NOT_FOUND_TOPIC);
                    res->SetId(msg->GetId());
                    res->SetType(Mtype::RSP_TOPIC);
                    conn->send(res);
                }
                void removeTopic(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    std::string topic_name = msg->topicKey();
                    Topic::ptr topic;
                    {
                        std::lock_guard<std::mutex> guard(_mutex);
                        auto iter = _topics.find(topic_name);
                        if(iter==_topics.end()){
                            return;
                        }
                        topic = iter->second;
                        _topics.erase(topic_name);
                    }
                    // Prevent two unrelated locks from affecting each other and causing performance degration
                    // so remove here, 
                    for(const Subscriber::ptr &subs: topic->subscribers){
                        subs->unsubscribe(topic_name);
                    }
                    return;

                }
                void createTopic(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    Topic::ptr topic = std::make_shared<Topic>(msg->topicKey());
                    std::lock_guard<std::mutex> guard(_mutex);
                    _topics.insert(std::make_pair(msg->topicKey(),topic));
                    return;
                }
                bool subscribeTopic(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    //if doesn't has this subscriber,add it
                    //if doesn't has this topic, response error
                    Subscriber::ptr subscriber;
                    Topic::ptr topic;
                    {
                        std::lock_guard<std::mutex> guard(_mutex);
                        auto iter_sub = _subscribers.find(conn);
                        auto iter_topic = _topics.find(msg->topicKey());
                        if(iter_sub ==_subscribers.end()){
                            auto subscriber = std::make_shared<Subscriber>(conn);
                            iter_sub = _subscribers.insert(std::make_pair(conn,subscriber)).first;
                        }
                        if(iter_topic != _topics.end()){
                            topic = iter_topic->second;
                        }
                        else{
                            return false;
                        }
                        subscriber = iter_sub->second;
                    }
                    subscriber->subscribe(msg->topicKey());
                    topic->appendSubscriber(subscriber);
                    return true;
                }
                void cancelTopic(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    //if doesn't has this subscriber, do nothing
                    //if doesn't has this topic, do nothing
                    Subscriber::ptr subscriber;
                    Topic::ptr topic;
                    {
                        std::lock_guard<std::mutex> guard(_mutex);
                        auto iter_sub = _subscribers.find(conn);
                        if(iter_sub == _subscribers.end()){
                            return;
                        }
                        subscriber = iter_sub->second;
                        auto iter_topic = _topics.find(msg->topicKey());
                        if(iter_topic != _topics.end()){
                            topic = iter_topic->second;
                        }
                        else{
                            return;
                        }
                    }
                    topic->removeSubscriber(subscriber);
                    subscriber->unsubscribe(msg->topicKey());
                    return;
                    
                }
                bool publishMsg(const ConnectionBase::ptr &conn,const TopicRequest::ptr &msg){
                    Topic::ptr topic;
                    {
                        std::lock_guard<std::mutex> guard(_mutex);
                        auto iter_topic = _topics.find(msg->topicKey());
                        if(iter_topic == _topics.end()){
                            return false;
                        }
                        topic = iter_topic->second;
                    }

                    topic->pushMessage(msg);
                    return true;
                }
                struct Subscriber{
                    using ptr = std::shared_ptr<Subscriber>;
                    ConnectionBase::ptr conn;
                    std::mutex mutex;
                    std::unordered_set<std::string> topics;           //topics subscribed by this people
                    //remove this people from topics subscribed by it when offline
                    Subscriber(const ConnectionBase::ptr &con):conn(con){}

                    //call when this person subscribe sometopic
                    void subscribe(const std::string &topic_name){
                        std::unique_lock<std::mutex> guard(mutex);
                        topics.insert(topic_name);
                    }
                    //call when sometopic is deleted or this person unsubscribe
                    void unsubscribe(const std::string &topic_name){
                        std::unique_lock<std::mutex> guard(mutex);
                        topics.erase(topic_name);
                    }
                };

                struct Topic{
                    using ptr = std::shared_ptr<Topic>;
                    std::string topic_name;
                    std::mutex mutex;
                    std::unordered_set<Subscriber::ptr> subscribers; //subscribers of this topic
                    Topic(const std::string &name):topic_name(name){}
                    //call when someone subscribe this topic
                    void appendSubscriber(const Subscriber::ptr &subscriber){
                        std::unique_lock<std::mutex> guard(mutex);
                        subscribers.insert(subscriber);
                    }
                    //call when someone unsubscribe this topic
                    void removeSubscriber(const Subscriber::ptr &subscriber){
                        std::unique_lock<std::mutex> guard(mutex);
                        subscribers.erase(subscriber);
                    }
                    //push to all subscribers when message push to this topic
                    void pushMessage(const MessageBase::ptr &msg){
                        std::unique_lock<std::mutex> guard(mutex);
                        if(subscribers.empty()){
                            std::cout<<topic_name<<" has no members";
                            return;
                        }
                        for(const auto &subscriber : subscribers){
                            subscriber->conn->send(msg);
                        }
                    }
                };
            private:
                std::mutex _mutex;
                //conn: subs
                std::unordered_map<std::string, Topic::ptr> _topics;
                std::unordered_map<ConnectionBase::ptr, Subscriber::ptr> _subscribers;
        };
    }
}