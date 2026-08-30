#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "util/cli.hpp"

namespace {

bool parse(const std::vector<std::string>& arguments, czgba::util::CliOptions& options, std::string& error)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    return czgba::util::parse_cli(static_cast<int>(argv.size()), argv.data(), options, error);
}

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "CLI profile smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    czgba::util::CliOptions defaults;
    std::string error;
    require(parse({"cardputer-zero-gba"}, defaults, error), "default invocation parses");
    require(defaults.device_profile == "cardputer-zero", "Cardputer Zero is the default profile");

    czgba::util::CliOptions k230;
    require(parse({"cardputer-zero-gba", "--device-profile", "tdvp-k230"}, k230, error),
            "TDVP K230 profile parses");
    require(k230.device_profile == "tdvp-k230", "TDVP K230 profile is selected");

    czgba::util::CliOptions stress;
    require(parse({"cardputer-zero-gba", "--present-delay-ms", "50"}, stress, error),
            "presentation-delay stress option parses");
    require(stress.present_delay_ms == 50, "presentation-delay value is preserved");

    czgba::util::CliOptions invalid_delay;
    require(!parse({"cardputer-zero-gba", "--present-delay-ms", "1001"}, invalid_delay, error),
            "out-of-range presentation delay is rejected");
    require(error.find("--present-delay-ms") != std::string::npos,
            "presentation-delay rejection names the option");

    czgba::util::CliOptions invalid;
    require(!parse({"cardputer-zero-gba", "--device-profile", "unknown"}, invalid, error),
            "unknown profile is rejected");
    require(error.find("--device-profile") != std::string::npos, "profile rejection names the option");
    return 0;
}
