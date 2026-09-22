#define _XOPEN_SOURCE 500

#include <iostream>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

using namespace std;
namespace logging = boost::log;

void InitLogger() {
  logging::add_common_attributes();
  logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");
  string log_format = "%TimeStamp% [%Severity%] - %Message%";

  logging::add_console_log(
    cout,
    logging::keywords::format = log_format
  );
}

int main(int argc, char **argv) {
  InitLogger();
  BOOST_LOG_TRIVIAL(trace) << "SOMETHING";
  return 0;
}
