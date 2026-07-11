# Codegen 预留入口。
#
# 当前不会实际运行 generator；这里只记录未来接入构建期 codegen 的 CMake 位置。
#
# 未来形态：
#   mdf_add_codegen_target(
#     TARGET md-adapter-binance-generated
#     INPUT  ${CMAKE_SOURCE_DIR}/path/to/provider.ir.json
#     OUTPUT_DIR ${CMAKE_SOURCE_DIR}/libs/adapters/crypto/binance/generated
#   )

function(mdf_add_codegen_target)
  message(FATAL_ERROR "mdf_add_codegen_target 是预留入口，当前尚未实现")
endfunction()
