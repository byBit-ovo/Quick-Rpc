#pragma once
#include <unordered_map>
#include <string>
#include "pb/message.pb.h"

namespace MyRpc
{
    // 使用 proto 生成的枚举 Mtype, Rcode, TopicOptype, ServiceOptype

    // ReqType 仅 C++ 使用，proto 中无
    enum class ReqType
    {
        REQ_ASYNC = 0,
        REQ_CALLBACK
    };

    // proto ServiceOptype 使用 SERVICE_UNKNOW_PB，此处保持兼容
    static std::unordered_map<int, std::string> RcodeDesc = {
        {static_cast<int>(Rcode::RCODE_OK), "成功处理！"},
        {static_cast<int>(Rcode::RCODE_PARSE_FAILED), "消息解析失败！"},
        {static_cast<int>(Rcode::RCODE_ERROR_MSGTYPE), "消息类型错误！"},
        {static_cast<int>(Rcode::RCODE_INVALID_MSG), "无效消息"},
        {static_cast<int>(Rcode::RCODE_DISCONNECTED), "连接已断开！"},
        {static_cast<int>(Rcode::RCODE_INVALID_PARAMS), "无效的Rpc参数!"},
        {static_cast<int>(Rcode::RCODE_NOT_FOUND_SERVICE), "没有找到对应的服务！"},
        {static_cast<int>(Rcode::RCODE_INVALID_OPTYPE), "无效的操作类型"},
        {static_cast<int>(Rcode::RCODE_NOT_FOUND_TOPIC), "没有找到对应的主题！"},
        {static_cast<int>(Rcode::RCODE_INTERNAL_ERROR), "内部错误！"},
        {static_cast<int>(Rcode::RCODE_INVALID_RESULT), "结果类型错误!"}
    };

    inline std::string ErrReason(Rcode code)
    {
        auto iter = RcodeDesc.find(static_cast<int>(code));
        if (iter == RcodeDesc.end())
        {
            return "未知错误";
        }
        return iter->second;
    }
}
