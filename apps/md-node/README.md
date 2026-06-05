# md-node 进程入口

`md-node` 是统一进程入口，使用角色参数决定当前进程承担哪些职责：

```bash
md-node --roles=gateway
md-node --roles=controller
md-node --roles=gateway,controller
```

早期可以只跑 gateway 角色读取静态配置；后续可以启用 controller 角色或接入外部协调系统。当前实现还是占位骨架。
