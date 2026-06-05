# Codegen hook placeholder.
#
# This file intentionally does not run any generator today. It documents the
# future CMake integration point for build-time code generation.
#
# Future shape:
#   mdf_add_codegen_target(
#     TARGET md-adapter-binance-generated
#     INPUT  ${CMAKE_SOURCE_DIR}/path/to/provider.ir.json
#     OUTPUT_DIR ${CMAKE_SOURCE_DIR}/libs/md-adapters/crypto/binance/generated
#   )

function(mdf_add_codegen_target)
  message(FATAL_ERROR "mdf_add_codegen_target is reserved but not implemented yet")
endfunction()
