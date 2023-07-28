# WebServer

使用C++实现的一个高性能服务器，通过webbench压力测试可以实现上万的QPS(Queries Per Second)；

但需要注意的是，QPS并不是衡量服务器的唯一指标，响应时间和并发连接数也是很重要的参考指标，只是本项目以QPS结果为参考；

## 功能

## 框架结构

## 运行环境
```
系统：Arch Linux
CPU：intel i5 1135G7
```
## 启动方式


## 压力测试

通过webbench进行压力测试，该工具的获取既可以基于源码安装，同时可以根据你自身的发行版安装！

### 获取方式
- **Arch Linux**

直接从万能的AUR中搜索下载即可：`yay -S webbench`

- **其他**
源码编译安装即可，[源码地址](http://ibiblio.org/pub/Linux/apps/www/servers/webbench-1.5.tar.gz)；


## 性能表现

## 注意事项
通过makefile编译程序之后，要在WebServer-Self的文件夹目录下运行服务器程序，否则无法获取到resources文件信息，导致浏览器访问失败；

## 致谢

TCP/IP网络编程 [韩] 尹圣雨 著	金国哲 译。

Linux高性能服务器编程，游双著。

@[markparticle](https://github.com/markparticle/WebServer)

@[qinguoyi](https://github.com/qinguoyi/TinyWebServer)