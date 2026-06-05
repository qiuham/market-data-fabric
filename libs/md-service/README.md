# md-service 服务运行时模块

`md-service` 是 gateway 的单进程运行时。

职责：

- 加载配置。
- 构建 adapter -> normalizer -> publisher pipeline。
- 管理 adapter 生命周期。
- 组装 recorder、publisher、metrics、health。
- 处理启动、停止、重连等服务级逻辑。

它不应该包含具体供应商字段转换逻辑。
