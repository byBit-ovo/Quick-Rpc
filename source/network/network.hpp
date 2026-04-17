#pragma once
#include "muduo/net/Buffer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/EventLoop.h"
#include <muduo/net/EventLoopThread.h>
#include "abstract.hpp"
#include <arpa/inet.h>
#include <unordered_map>
#include <mutex>
namespace MyRpc
{
    class MuduoBuffer : public BufferBase
    {
    public:
        using ptr = std::shared_ptr<MuduoBuffer>;
        MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}
        virtual size_t readableSize()override
        {
            return _buf->readableBytes();
        }
        virtual int peekInt32() override
        {
            return _buf->peekInt32();
        }
        virtual void retrieveInt32()override
        {
            _buf->retrieveInt32();
        }
        virtual int readInt32()override
        {
            return _buf->readInt32();
        }
        virtual std::string readAsString(size_t len)override
        {
            return _buf->retrieveAsString(len);
        }
        virtual std::string retrieveAllAsString()override
        {
            return _buf->retrieveAllAsString();
        }

    private:
        muduo::net::Buffer *_buf;
    };

    class BufferFactory
    {
    public:
        template <class... Args>
        static BufferBase::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };


    // Tcp编程要考虑粘包和拆包问题，所以需要一个协议来解决这个问题
    class RpcProtocol : public ProtocolBase
    {
    public:
        using ptr = std::shared_ptr<RpcProtocol>;
        // 判断缓冲区是否存在一条完整的消息
        // 8 message(8 bytes)
        //|--headLen--|--Mtype--|--idLen-- |------id--------|--body--|
        //|--4bytes---|--4bytes-|--4bytes--|--idlen bytes---|--body--|
        //|--4bytes---|-----------------headLen bytes----------------|
        //19461570560603979776182ee89f-9d8a-a504-0000-000000000001
        virtual bool canProceed(const BufferBase::ptr &buffer) override
        {
            if(buffer->readableSize() < _headLen){
                return false;
            }
            int32_t len = buffer->peekInt32();
            // DLOG("canProceed: headLen: %d", len);
            // DLOG("canProceed: buffer->size(): %ld", buffer->readableSize());
            return buffer->readableSize() >= len + _headLen;
        }
        virtual bool recieveAmessage(const BufferBase::ptr &buffer, MessageBase::ptr &msg) override
        {
            // 在调用此函数之前需要先判断canProceed
            // Muduo 提取接口自动将网络序转换为字节序，所以这里不需要转换
            int32_t len = buffer->readInt32();
            // DLOG("recieveAmessage : headLen: %d",len);
            Mtype mtype = static_cast<Mtype>(buffer->readInt32());
            // DLOG("recieveAmessage : Mtype: %d",static_cast<int>(mtype));
            int32_t idlen = buffer->readInt32();
            // DLOG("recieveAmessage : LenOfId: %d",idlen);
            std::string id = buffer->readAsString(idlen);
            std::string body = buffer->readAsString(len - _typeLen - _idLen - idlen);
            msg = MyRpc::MessageFactory::create(static_cast<Mtype>(mtype));
            if (msg.get() == nullptr)
            {
                ELOG("消息类型错误,解析失败!");
                return false;
            }
            msg->deserialize(body);
            msg->SetId(id);
            msg->SetType(mtype);
            return true;
        }
        virtual std::string serialize(const MessageBase::ptr &msg)override
        {
            //|--headLen--|--Mtype--|--idLen-- |------id--------|--body--|
            //48-1-23-0002223274755-8833-{"optype": "publish","Method":"Add"}
            std::string body = msg->serialize();
            std::string id = msg->GetId();
            int32_t mtype = htonl(static_cast<int>(msg->GetType()));
            int32_t hlen = _typeLen + _idLen + id.size() + body.size();
            int32_t nlen = htonl(hlen);
            int32_t idlen_h = id.size();
            int32_t idlen_n = htonl(idlen_h);
            // 错误，整形不能以字符串的形式传输，必须以内存中实际按照Int固定大小字节的方式传输
            // std::stringstream ss;
            // ss << nlen << mtype << idlen_n << id << body;
            // return ss.str();

            // DLOG("headLen :%d",hlen);
            // DLOG("Mtype :%d",mtype);
            // DLOG("id.size() :%ld",id.size());
            std::string mess;
            mess.reserve(hlen);
            mess.append((char*)&nlen, _headLen);
            mess.append((char*)&mtype, _typeLen);
            mess.append((char*)&idlen_n, _idLen);
            //字符串本来就是字节流，不存在字节顺序的问题
            mess.append(id);
            mess.append(body);
            return mess;
        }

    private:
        static const int32_t _headLen;
        static const int32_t _typeLen;
        static const int32_t _idLen;
    };
    const int32_t RpcProtocol::_headLen = sizeof(int);
    const int32_t RpcProtocol::_typeLen = sizeof(int);
    const int32_t RpcProtocol::_idLen = sizeof(int);

    class ProtocolFactory
    {
    public:
        template <class... Args>
        static ProtocolBase::ptr create(Args... args)
        {
            return std::make_shared<RpcProtocol>(std::forward<Args>(args)...);
        }
    };

    class RpcConnection : public ConnectionBase
    {
    public:
        using ptr = std::shared_ptr<RpcConnection>;
        RpcConnection(const ProtocolBase::ptr &protocol, const muduo::net::TcpConnectionPtr &conn) : _protocol(protocol), _conn(conn) {}

        virtual void send(const MessageBase::ptr &msg) override
        {
            std::string message = _protocol->serialize(msg);
            // DLOG("序列化数据：%s", message.c_str());
            _conn->send(message);
        }
        virtual void shutDown() override
        {
            _conn->shutdown();
        }
        virtual bool isConnected() override
        {
            return _conn->connected();
        }

    private:
        ProtocolBase::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };
    class ConnectionFactory
    {
    public:
        template <class... Args>
        static ConnectionBase::ptr create(Args... args)
        {
            return std::make_shared<RpcConnection>(std::forward<Args>(args)...);
        }
    };

    class MuduoServer : public ServerBase
    {
    public:
        using ptr = std::shared_ptr<MuduoServer>;
        MuduoServer(int port) : _tcpsvr(&_baseloop, muduo::net::InetAddress("0.0.0.0", port),
                                        "MuduoServer", muduo::net::TcpServer::kReusePort),
                                        _protocol(ProtocolFactory::create())
        {}

        virtual void start()
        {
            _tcpsvr.setConnectionCallback(std::bind(&MuduoServer::ConnectionCallBack, this, std::placeholders::_1));
            _tcpsvr.setMessageCallback(std::bind(&MuduoServer::MessageCallBack, this,
                                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _tcpsvr.start();
            _baseloop.loop();
        }

    private:
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _tcpsvr;
        ProtocolBase::ptr _protocol;
        std::unordered_map<muduo::net::TcpConnectionPtr, ConnectionBase::ptr> _connections;
        std::mutex _lock;
        static const size_t msgMaxLen;
        void ConnectionCallBack(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                ILOG("新链接建立成功!");
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    _connections.insert(std::make_pair(conn, ConnectionFactory::create(_protocol, conn)));
                }
                if (_connection_call_back)
                {
                    _connection_call_back(_connections[conn]);
                }
            }
            else
            {
                ConnectionBase::ptr rpcConn;
                ILOG("连接断开!");
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto iter = _connections.find(conn);
                    if (iter == _connections.end())
                    {
                        return;
                    }
                    rpcConn = iter->second;
                    _connections.erase(conn);
                }
                if (_close_call_back)
                {
                    _close_call_back(rpcConn);
                }
            }
        }
        void MessageCallBack(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            BufferBase::ptr buf_base = BufferFactory::create(buf);
            while (true)
            {
                if (_protocol->canProceed(buf_base) == false)
                {
                    ILOG("消息不够一条完整的数据");
                    if (buf_base->readableSize() >= msgMaxLen)
                    {
                        conn->shutdown();
                        ELOG("缓冲区数据过长!连接关闭!");
                        return;
                    }
                    break;
                }
                MessageBase::ptr msg;
                if (_protocol->recieveAmessage(buf_base, msg) == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区数据解析错误!连接关闭!");
                    break;
                }
                ConnectionBase::ptr rpcConn;
                {
                    std::lock_guard<std::mutex> guard(_lock);
                    auto iter = _connections.find(conn);
                    if (iter == _connections.end())
                    {
                        ELOG("连接关系错误,找不到对应关系!连接关闭!");
                        conn->shutdown();
                        return;
                    }
                    rpcConn = iter->second;
                }
                DLOG("MessageCallBack: 接收到一条完整的请求");
                if (_message_call_back)
                {
                    _message_call_back(rpcConn, msg);
                }
            }
        }
    };
    const size_t MuduoServer::msgMaxLen = (1 << 16);
    class ServerFactory
    {
    public:
        template <typename... Args>
        static ServerBase::ptr create(Args... args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    class MuduoClient : public ClientBase
    {
    public:
        MuduoClient(const std::string &ip, int port) : _baseloop(_thread.startLoop()),
            _protocol(ProtocolFactory::create()),
            _client(_baseloop, muduo::net::InetAddress(ip, port), "client"), _cdl(1) {}
        virtual void connect() override
        {
            DLOG("设置回调，连接服务器!");
            _client.setConnectionCallback(std::bind(&MuduoClient::ConnectionCallBack, this, std::placeholders::_1));
            _client.setMessageCallback(std::bind(&MuduoClient::MessageCallBack, this, std::placeholders::_1,
                                                 std::placeholders::_2, std::placeholders::_3));
            _client.connect();
            // 等待所有子线程就绪，为0时唤醒
            _cdl.wait();
        }
        virtual void shutDown()override{
            _client.disconnect();
        }
        virtual void send(const MessageBase::ptr &msg)override
        {
            if(connected()){
                _conn->send(msg);
            }
            else{
                DLOG("发送消息失败,连接尚未建立!");
            }
        }
        virtual bool connected(){
            return (_conn && _conn->isConnected());
        }
        virtual ConnectionBase::ptr connection()override
        {
            if(_conn.get() == nullptr){
                ELOG("客户端连接尚未建立,_conn.get()为nullptr");
            }
            return _conn;
        }

    private:
        void ConnectionCallBack(const muduo::net::TcpConnectionPtr &conn)
        {
            DLOG("进入ConnectionCallBack");
            if (conn->connected())
            {
                // if(conn.get() != nullptr){
                //     DLOG("TcpConnection不为空!");
                // }
                // else{
                //     DLOG("TcpConnection为空!");

                // }
                //--count,表示已经有一个资源就绪
                _conn = ConnectionFactory::create(_protocol, conn);
                if(_connection_call_back){
                    _connection_call_back(_conn);
                }
                //!!!!!!!!!这里一定注意先给_conn赋值，再countDown,否则唤醒的主线程可能拿到空数据导致segement fault !!!!!!!!!!!!!!
                _cdl.countDown();
            }
            else
            {
                ILOG("断开服务器连接");
                _conn.reset();
                if(_close_call_back){
                    _close_call_back(_conn);
                }
            }
        }
        void MessageCallBack(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            BufferBase::ptr buf_base = BufferFactory::create(buf);
            while (true)
            {
                if (_protocol->canProceed(buf_base) == false)
                {
                    
                    if (buf_base->readableSize() >= msgMaxLen)
                    { 
                        conn->shutdown();
                        ELOG("缓冲区数据过长!连接关闭!");
                        return;
                    }
                    break;
                }
                MessageBase::ptr msg;
                if (_protocol->recieveAmessage(buf_base, msg) == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区数据解析错误!连接关闭!");
                    break;
                }
                DLOG("收到完整消息");
                if (_message_call_back)
                {
                    _message_call_back(_conn, msg);
                }
            }
        }
        muduo::net::EventLoopThread _thread;
        muduo::net::EventLoop *_baseloop;
        ProtocolBase::ptr _protocol;
        ConnectionBase::ptr _conn;
        muduo::net::TcpClient _client;
        muduo::CountDownLatch _cdl;
        static const size_t msgMaxLen;
    };
    const size_t MuduoClient::msgMaxLen = (1<<12);

    class ClientFactory
    {
        public:
            template<typename ...Args>
            static ClientBase::ptr create(Args ...args)
            {
                return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
            }
    };
}