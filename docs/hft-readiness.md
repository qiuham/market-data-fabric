# 高频准备度规划

本项目第一目标是中频量化系统的统一行情接入和分发，但核心库需要为未来低延迟和高频场景保留复用空间。原则是：提前规划钩子，但不要让普通部署被高频优化绑架。

## 边界

高频或低延迟 pipeline 可以复用：

- `md-core` 事件类型、header、ID、序号和 sink 概念。
- `md-refdata` 的轻量 symbol/precision 查询。
- `md-book` 盘口构建。
- 部分 `md-codecs` 和 feed decoder。
- `md-runtime` 低延迟运行时辅助能力。

不应该依赖：

- NATS 或 Kafka。
- service supervisor（服务监管器）。
- 控制面 watch、lease 或动态调度。
- 阻塞日志、阻塞指标、数据库调用。
- 热路径中的动态分配和字符串处理。

## CPU 和线程放置

规划能力：

- 将 feed IO、decoder、book builder、publisher、recorder 线程绑定到指定 CPU。
- 将控制面和 metrics 线程放到非热路径 CPU。
- 在 Linux 生产环境中通过 `pthread_setaffinity_np` 等能力实现 CPU 绑核。
- 通过配置描述每类线程的 CPU、调度策略和优先级。

示例：

```text
feed_io      -> CPU 2
decoder      -> CPU 3
book_builder -> CPU 4
publisher    -> CPU 5
recorder     -> CPU 8
controller   -> CPU 9
metrics      -> CPU 9
```

## 计时器和时间戳

需要明确区分：

- 行情源时间戳。
- 本机接收时间戳。
- 标准化完成时间戳。
- 发布前时间戳。
- 消费端接收时间戳。

规划能力：

- 默认使用适合延迟测量的单调时钟。
- 为 TSC reader 预留接口，但只有在 CPU/TSC 稳定性验证后启用。
- 为 PHC/NIC 硬件时间戳预留接口。
- 跨机器延迟分析需要时间同步体系支持。

不要混用“业务时间正确性”和“本地延迟测量”，每个时间戳都要记录来源语义。

## 内存和分配

规划能力：

- 热路径对象池和 ring buffer 预分配。
- 使用固定长度结构和 fixed-point 价格/数量。
- 避免热回调中的 `std::string`、JSON 对象、堆分配和阻塞日志。
- 对热点结构和 ring slot 做 cache line 对齐。
- 在生产低延迟 profile 中支持 page lock，降低 page fault 风险。
- huge page 作为可选能力，必须经过 benchmark 后再启用。

## 队列和背压

规划模型：

- 能用 SPSC ring 就优先使用 SPSC ring。
- 所有队列必须有界，不能无限增长。
- 慢消费者不能阻塞 feed ingestion。
- 背压策略必须显式配置：丢弃、跳到最新、断开消费者、转入持久化流等。

## 网络

规划能力：

- UDP 组播只有在 IDC/VLAN 能力确认后再启用。
- 对 UDP feed 预留 batch receive 能力。
- 对 TCP 和 socket 选项预留 profile。
- 生产主机需要单独文档说明 NIC RSS、IRQ 亲和性和内核参数。
- 为未来 AF_XDP、DPDK 或其他 kernel-bypass 方案保留扩展点。

这些能力属于 transport/runtime 层，不属于 `md-core`。

## 观测但不伤害热路径

延迟指标应该通过采样或无锁侧通道采集，避免在热回调中调用阻塞 exporter。至少记录：

- source 到 receive 延迟。
- receive 到 publish 延迟。
- publish 到 consume 延迟。
- sequence gap、duplicate、stale stream、重连次数、book 校验失败。

## 配置示例

```yaml
runtime_profile: low_latency
threads:
  feed_io:
    cpu: 2
    scheduler: fifo
    priority: 70
  decoder:
    cpu: 3
    scheduler: fifo
    priority: 65
memory:
  preallocate: true
  lock_pages: true
  prefer_huge_pages: false
clock:
  source: monotonic_raw
  require_ptp_sync: true
```

## 开发和生产验证

核心逻辑、解析器和单元测试可以在普通开发环境中完成。低延迟性能结论必须在目标生产机器、目标内核、目标网卡、目标时钟同步和目标部署拓扑下验证。
