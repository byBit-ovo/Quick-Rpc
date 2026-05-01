# Json-Rpc 生产问题修复记录

## 1. `network.hpp`

### 问题一：静态成员 ODR 违规（链接期致命错误）

`RpcProtocol`、`MuduoServer`、`MuduoClient` 的静态常量成员在头文件中进行了类外定义，头文件被多个翻译单元包含时产生重复定义，链接报错。

**修改：** 将类外定义删除，改为类内 `static constexpr` 内联定义。

```cpp
// Before
static const int32_t _headLen;
// ...
const int32_t RpcProtocol::_headLen = sizeof(int);  // 类外，ODR 违规

// After
static constexpr int32_t _headLen = sizeof(int32_t);  // 类内，无 ODR 问题
```

---

### 问题二：客户端 `msgMaxLen` 与服务端不一致

| | 修改前 | 修改后 |
|---|---|---|
| `MuduoServer::msgMaxLen` | `1 << 16`（64 KB） | `1 << 16`（64 KB）|
| `MuduoClient::msgMaxLen` | `1 << 12`（**4 KB**）| `1 << 16`（64 KB）|

服务端发出的合法响应超过 4 KB 时，客户端会将其判定为异常并强制断连。

---

### 问题三：`close callback` 收到 `nullptr`

```cpp
// Before
_conn.reset();
_close_call_back(_conn);  // _conn 已是 nullptr，上层拿到空指针

// After
ConnectionBase::ptr closed_conn = _conn;  // 先保存
_conn.reset();
_close_call_back(closed_conn);            // 传入断连前的有效对象
```

---

## 2. `requestor.hpp`

### 问题四：同步调用永久阻塞

`send` 直接调用 `future::get()`，服务端宕机或网络中断时调用方线程永久挂起。

```cpp
// Before
resp = resp_future.get();  // 无超时，永久阻塞

// After
if (resp_future.wait_for(std::chrono::milliseconds(timeout_ms))
        == std::future_status::timeout) {
    removeDesc(msg->GetId());
    ELOG("RPC请求超时! id: %s", msg->GetId().c_str());
    return false;
}
resp = resp_future.get();
```

默认超时 5 秒，可通过 `timeout_ms` 参数调整。

---

### 问题五：断连导致 `broken_promise` crash

连接断开后，`_requests_desc` 中残留的 `std::promise` 在析构时未设值，关联 `future::get()` 抛出 `std::future_error`，若上层未捕获则进程崩溃。

新增 `notifyDisconnect()`，在连接断开时由上层调用：

```cpp
void notifyDisconnect() {
    std::unique_lock<std::mutex> guard(_lock);
    for (auto& [id, desc] : _requests_desc) {
        if (desc->_type == ReqType::REQ_ASYNC) {
            try {
                desc->_response.set_exception(std::make_exception_ptr(
                    std::runtime_error("连接已断开")));
            } catch (...) {}
        }
    }
    _requests_desc.clear();
}
```

---

## 3. `rpc_router.hpp`

### 问题六：用户注册的服务回调抛异常导致服务器崩溃

业务函数抛出的异常沿调用栈传播至 muduo IO 线程，进程终止。

```cpp
// Before
_call(params, result);  // 异常直接穿透

// After
try {
    _call(params, result);
    return true;
} catch (const std::exception& e) {
    ELOG("服务调用异常: %s", e.what());
} catch (...) {
    ELOG("服务调用未知异常");
}
return false;
```

异常被捕获后，`RpcRouter` 正常向客户端回复 `RCODE_INVALID_RESULT` 错误码，服务进程不受影响。
