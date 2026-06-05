# md-transport 传输层模块

`md-transport` 负责标准化行情事件的发布和订阅。

计划支持：

- NATS：适合中频服务化 pub/sub。
- Kafka：适合持久化流、研究、回放、ETL。
- 共享内存：适合同机低延迟 C++ 消费者。
- TCP fanout：适合简单部署。
- UDP 组播：适合明确支持组播的 IDC/VLAN 环境。
- file：适合录制和调试。

传输层不负责解析外部行情，也不负责供应商字段转换。
