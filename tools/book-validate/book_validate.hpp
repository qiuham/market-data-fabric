#pragma once

// 验证器实现保持工具私有；公共 marketdata API 不暴露 Parquet、快照或报表类型。
int book_validate_main(int argc, char** argv);
