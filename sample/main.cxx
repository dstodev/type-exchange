#define _CRT_SECURE_NO_WARNINGS 1
#include <cstdlib>  // for std::getenv
#undef _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <optional>
#include <string>

#include <cli.hxx>
#include <project.hxx>

#include <type-exchange.hxx>

using namespace project;

void print_int(int value)
{
	std::cout << value << std::endl;
}

struct NonCopyable
{
	NonCopyable(NonCopyable const&) = delete;
	NonCopyable& operator=(NonCopyable const&) = delete;

	NonCopyable(NonCopyable&&) = default;
	NonCopyable& operator=(NonCopyable&&) = default;

	NonCopyable() = default;
};

namespace {
void set_log_level(std::optional<std::string> const& cli_level = std::nullopt);
auto get_env_var(char const* name) -> std::optional<std::string>;
}  // namespace

int main(int const argc, char const* argv[])
{
	Cli const cli {argc, argv};  // May call std::exit(); see Cli constructor documentation.

#if ENABLE_LOGGING
	set_log_level(cli.log_level());
	log::print_enabled_levels();
#endif

	std::cout << "Welcome!\n" << std::endl;

	TypeExchange exchange;

	exchange.subscribe<int>([](int const& message) { std::cout << "Received int: " << message << std::endl; });
	exchange.subscribe<int>(print_int);

	exchange.subscribe<std::string>(
	    [](std::string const& message) { std::cout << "Received string: " << message << std::endl; });

	exchange.publish(1);
	exchange.publish(2);
	exchange.publish(std::string {"Hello, World!"});

	char test = 'a';

	exchange.subscribe<char>([&test](char const& message) { test = message; });
	exchange.publish('b');

	std::cout << "Var before processing: " << test << std::endl;

	NonCopyable nc;

	exchange.publish(std::move(nc));

	exchange.subscribe<NonCopyable>(
	    [](NonCopyable const& message) { std::cout << "Received NonCopyable!" << std::endl; });

	std::cout << "-- Processing..." << std::endl;
	exchange.process_messages();
	std::cout << "-- Done processing." << std::endl;

	std::cout << "Var after processing: " << test << '\n' << std::endl;

	return 0;
}

namespace {

void set_log_level(std::optional<std::string> const& cli_level)
{
	log::Level level {log::Level::None};

	// Give CLI precedence over environment variable.
	if (cli_level) {
		level = log::level_from(cli_level.value());
	}
	else if (auto const env_level {get_env_var("LOG_LEVEL")}) {
		level = log::level_from(env_level.value());
	}

	log::set_level(level);
}

auto get_env_var(char const* name) -> std::optional<std::string>
{
	char const* value {std::getenv(name)};
	return value ? std::make_optional(value) : std::nullopt;
}

}  // namespace
