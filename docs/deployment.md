# 部署说明

## 第一阶段

第一版建议使用静态 YAML 分配，每个 source 或 shard 单独启动一个 `marketdata-gateway`：

```bash
marketdata-gateway --roles=gateway --config configs/prod/crypto/binance_spot.yaml
marketdata-gateway --roles=gateway --config configs/prod/cn/futures/ctp.yaml
```

这种方式简单、可调试，适合先把 provider、标准化、publisher 和消费端流程跑通。

## 第二阶段

接入逻辑控制面能力：

- 节点注册。
- 心跳上报。
- assignment watch（分配变更监听）。
- lease 获取和续约。
- primary/standby 切换。
- publisher epoch 递增。

实现方式可以是：

- Kubernetes Lease / ConfigMap / CRD：适合 Kubernetes 部署环境。
- etcd：适合裸机或自建控制面。
- Consul：适合已有服务发现和 KV 体系的环境。
- 内置 controller 角色。

## 第三阶段

当 source、symbol、节点、机房、账号和供应商数量变多后，再考虑更完整的调度能力：

- 自动分片。
- 节点 drain。
- 主备切换策略。
- 动态订阅下发。
- 滚动升级和灰度。
- 管理 API 或 UI。

## 角色组合

小规模部署可以合并角色：

```text
node-a: gateway,controller
node-b: gateway,controller
node-c: gateway,controller
```

规模变大后建议分离：

```text
controller-0: controller
controller-1: controller
controller-2: controller

gateway-binance-0: gateway
gateway-binance-1: gateway
gateway-ctp-0: gateway
gateway-ib-0: gateway
```

## 数据面不经过控制面

无论控制面如何部署，实时行情都应该走数据面：

```text
provider -> decoder -> mapper -> transport
```

不要让每条行情进入 lease、assignment 或共识路径。
