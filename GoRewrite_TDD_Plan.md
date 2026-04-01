# WebServer-Self Go Rewrite — TDD 测试计划

## 1. 项目概述

将 C++ WebServer 重写为 Go 版本，保持所有原有功能：

- HTTP 服务器（支持 ET/LT 模式）
- 线程池并发处理
- 定时器（min-heap）
- MySQL 连接池
- 异步日志系统
- HTTP 请求解析 / 响应构建
- 静态文件服务
- 用户注册/登录（MySQL）

## 2. 目录结构

```
webserver-go/
├── config.toml              # 配置文件
├── go.mod
├── go.sum
├── cmd/
│   └── server/
│       └── main.go          # 程序入口
├── internal/
│   ├── config/
│   │   └── config.go        # 配置加载
│   ├── server/
│   │   └── server.go        # 主服务器
│   ├── epoll/
│   │   └── epoll.go         # Epoll IO 多路复用
│   ├── timer/
│   │   └── timer.go         # Min-heap 定时器
│   ├── threadpool/
│   │   └── threadpool.go    # 线程池
│   ├── buffer/
│   │   └── buffer.go        # 动态缓冲区
│   ├── http/
│   │   ├── request.go       # HTTP 请求解析
│   │   ├── response.go      # HTTP 响应构建
│   │   └── conn.go          # HTTP 连接处理
│   ├── sqlpool/
│   │   └── sqlpool.go       # MySQL 连接池
│   └── log/
│       └── logger.go        # 异步日志
├── resources/               # 静态资源（从原项目复制）
│   ├── index.html
│   ├── login.html
│   └── ...
└── tests/
    ├── buffer_test.go
    ├── timer_test.go
    ├── threadpool_test.go
    ├── request_test.go
    ├── response_test.go
    └── server_test.go
```

## 3. TDD 阶段划分

### 阶段一：基础设施（无外部依赖）

| 顺序 | 任务 | 测试内容 |
|------|------|----------|
| 1 | Buffer | NewBuffer, Write, Read, Peek, Retrieve, ReadFd, WriteFd |
| 2 | Timer | Add, Adjust, Del, Tick, GetNextTick, Pop |
| 3 | ThreadPool | Submit, WaitAll, Shutdown |

### 阶段二：HTTP 层

| 顺序 | 任务 | 测试内容 |
|------|------|----------|
| 4 | HttpRequest.parseRequestLine | 方法/路径/版本解析，正则验证 |
| 5 | HttpRequest.parseHeader | Header 解析，键值对提取 |
| 6 | HttpRequest.parseBody | POST body URL decode，用户名密码提取 |
| 7 | HttpRequest 完整流程 | 状态机：REQUEST_LINE→HEADERS→BODY→FINISH |
| 8 | HttpResponse.MakeResponse | 状态行/Header/Content，文件映射 |
| 9 | HttpResponse 错误码 | 200/400/403/404 处理 |
| 10 | HttpConn.process | read→parse→makeResponse→write 完整流程 |

### 阶段三：服务器集成

| 顺序 | 任务 | 测试内容 |
|------|------|----------|
| 11 | Epoll | AddFd, ModFd, DelFd, Wait |
| 12 | MySQL Pool | Init, GetConn, FreeConn, Close |
| 13 | Logger | 同步写、异步写、级别过滤 |
| 14 | Server.Start | Listen→Accept→Echo 端到端 |

## 4. 配置设计（config.toml）

```toml
[server]
port = 1316
trig_mode = 3        # 0=默认LT, 1=conn ET, 2=listen ET, 3=全ET
timeout_ms = 60000
open_linger = false

[database]
host = "localhost"
port = 3306
username = "ping"
password = "ping"
db_name = "login_info"
pool_size = 12

[threadpool]
thread_count = 6

[log]
open_log = true
level = 1
path = "./log"
suffix = ".log"
queue_size = 1024
```

## 5. 覆盖率目标

- 单元测试覆盖率 ≥ 80%
- 关键路径（parse、process、timer）覆盖率 100%
