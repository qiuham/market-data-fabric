# libs 库目录

这里放可复用 C++ 库。

依赖方向应尽量从外层指向内层：外层可以依赖 `md-core`，但 `md-core` 不能依赖 adapter、transport、service、cluster 或具体供应商 SDK。
