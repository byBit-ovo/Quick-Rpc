#pragma once
#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <sstream>
#include <atomic>
#include <iomanip>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <unordered_map>
#define LERR 0
#define LINF 1
#define LDBG 2
//等级小于等于默认等级才打印
#define LDFT LDBG
FILE *LogPlace = stdout;
#define LOG(level, format, ...)                                                                                   \
    do                                                                                                            \
    {                                                                                                             \
        if (level > LDFT)                                                                                         \
        {                                                                                                         \
            break;                                                                                                \
        }                                                                                                         \
        time_t now = time(nullptr);                                                                               \
        struct tm *cur_time = localtime(&now);                                                                    \
        fprintf(LogPlace, "%s-[%s: %d]-[%04d-%02d-%02d %02d:%02d:%02d] " format "\n", #level, __FILE__, __LINE__, \
                cur_time->tm_year + 1900, cur_time->tm_mon + 1,                                                   \
                cur_time->tm_mday, cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec, ##__VA_ARGS__);         \
    } while (0)

#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__)
class FileLog
{
private:
    FILE *_file;

public:
    FileLog()
    {
        ILOG("filelog 构造");
        _file = fopen(".log.txt", "a");
        LogPlace = _file;
    }
    ~FileLog()
    {
        fclose(_file);
    }
};
// FileLog toFile;
#include <google/protobuf/struct.pb.h>

class PbUtil
{
public:
    static void SetNumber(google::protobuf::Struct* s, const std::string& key, double val)
    {
        (*s->mutable_fields())[key].set_number_value(val);
    }
    static void SetString(google::protobuf::Struct* s, const std::string& key, const std::string& val)
    {
        (*s->mutable_fields())[key].set_string_value(val);
    }
    static void SetInt(google::protobuf::Struct* s, const std::string& key, int val)
    {
        (*s->mutable_fields())[key].set_number_value(static_cast<double>(val));
    }
    static double GetNumber(const google::protobuf::Value& v) { return v.number_value(); }
    static std::string GetString(const google::protobuf::Value& v) { return v.string_value(); }
    static int GetInt(const google::protobuf::Value& v) { return static_cast<int>(v.number_value()); }
};

class Uuid
{
public:
    static std::string uuid()
    {
        std::stringstream ss;
        // 1. 构造⼀个机器随机数对象
        std::random_device rd;
        // 2. 以机器随机数为种⼦构造伪随机数对象
        std::mt19937 generator(rd());
        // 3. 构造限定数据范围的对象
        std::uniform_int_distribution<int> distribution(0, 255);
        // 4. ⽣成8个16进制随机数，01 0a a1 ...
        for (int i = 0; i < 8; i++)
        {
            if (i == 4 || i == 6)
                ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
        }
        ss << "-";
        // 5. 定义⼀个8字节序号，逐字节组织成为16进制数字字符的字符串
        static std::atomic<size_t> seq(1); // 00 00 00 00 00 00 00 01
        size_t cur = seq.fetch_add(1);
        for (int i = 7; i >= 0; i--)
        {
            if (i == 5)
                ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xFF);
        }
        return ss.str();
    }
};

