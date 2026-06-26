#include "binance_live_runner.hpp"
#include "cli_options.hpp"

#include "md/runtime/logging.hpp"

#include <string>
#include <string_view>

int main(int argc, char **argv) {
  namespace app = md::apps::md_node;

  md::runtime::init_logging(app::parse_log_config(argc, argv));

  for (int i = 1; i < argc; ++i) {
    const std::string_view command{argv[i]};
    if (app::is_binance_command(command)) {
      return app::run_binance_command(command, argc, argv);
    }
  }

  MDF_LOG_INFO("md-node 行情节点");
  MDF_LOG_INFO("角色: gateway, controller, 或 gateway,controller");
  app::log_binance_usage();

  std::string args_line{"参数:"};
  for (int i = 1; i < argc; ++i) {
    args_line += ' ';
    args_line += argv[i];
  }
  MDF_LOG_INFO("{}", args_line);
  return 0;
}
