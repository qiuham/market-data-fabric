# md-codegen

这里预留构建期 codegen 工具。

目标：从外部 API 文档项目产出的稳定 IR 生成 C++ provider message type、mapper 骨架、测试骨架和文档。生成的 C++ 代码编译进具体 provider adapter，运行时不解释配置文件。

当前状态：只预留目录，不实现生成器。

## 输入

输入来自外部 API 文档项目，而不是本仓库手写 mapping 文件。具体 IR 格式等第一个真实 adapter 接入后再确定。

## 输出

建议输出到具体 provider 目录：

```text
libs/md-adapters/{asset_class}/{provider}/generated/
  *_provider_message.hpp
  *_mapper.generated.hpp
  *_mapper.generated.cpp
```

## 原则

- codegen 只在构建期运行。
- 运行时执行普通 C++。
- 复杂交易语义保留手写。
- 生成物可以提交仓库，并由 CI 检查是否和输入定义一致。
