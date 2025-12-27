#pragma once

#include <iuring/SocketPortID.hpp>
#include <string>

namespace http {
class ServerConfig {

public:
  iuring::SocketPortID web_port = iuring::SocketPortID::LOCAL_WEB_PORT;
  std::string server_certificate_file_path = "certs/myserver.crt";
  std::string CA_certificate_file_path = "certs/ca/ca.crt";
  std::string server_key_file_path = "certs/myserver.key";
  std::string server_key_password;
  bool use_tls = false;
};
} // namespace http