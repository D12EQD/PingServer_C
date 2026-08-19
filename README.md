# PingServer_C
C语言搭建的服务器
单线程，支持epoll，支持多客户端连接，支持异步发送数据，支持连接超时检测

# 实现步骤：
第一步：完成 tcp_server_create() 和基础socket初始化
第二步：实现 tcp_server_run() 中的epoll循环框架
第三步：处理 EPOLLIN on listen_fd → accept新连接
第四步：处理 EPOLLIN on conn_fd → recv + 业务逻辑
第五步：处理 EPOLLOUT → 异步发送缓冲中的数据
第六步：添加连接超时检测和资源清理