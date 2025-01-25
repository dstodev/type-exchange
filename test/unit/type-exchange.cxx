#include <gtest/gtest.h>

#include <type-exchange.hxx>

using namespace project;

namespace {
int INT_VALUE {0};
void assign_global_int(int value)
{
	INT_VALUE = value;
}

struct NonCopyable
{
	NonCopyable(NonCopyable const&) = delete;
	NonCopyable& operator=(NonCopyable const&) = delete;
	NonCopyable(NonCopyable&&) = default;
	NonCopyable& operator=(NonCopyable&&) = default;
	NonCopyable() = default;
};

struct NonMoveable
{
	NonMoveable(NonMoveable const&) = default;
	NonMoveable& operator=(NonMoveable const&) = default;
	NonMoveable(NonMoveable&&) = delete;
	NonMoveable& operator=(NonMoveable&&) = delete;
	NonMoveable() = default;
};

struct Receiver
{
	int _value {};
	std::string _str {};

	void receive(int value)
	{
		_value += value;
	}
	void receive(std::string const& str)
	{
		_str += str;
	}
};
}  // namespace

TEST(TypeExchange, construct)
{
	TypeExchange exchange;
	(void) exchange;
}

TEST(TypeExchange, subscribe_only_lambda)
{
	TypeExchange exchange;
	exchange.subscribe<int>([](int const& message) { (void) message; });
}

TEST(TypeExchange, subscribe_only_function)
{
	TypeExchange exchange;
	exchange.subscribe<int>(assign_global_int);
}

TEST(TypeExchange, publish_only)
{
	TypeExchange exchange;
	exchange.publish(1);
}

TEST(TypeExchange, subscribe_and_publish)
{
	TypeExchange exchange;

	int value {0};

	exchange.subscribe<int>([&value](int const& message) { value = message; });  // lambda
	exchange.subscribe<int>(assign_global_int);  // function

	exchange.publish(1);

	ASSERT_EQ(0, value);
	ASSERT_EQ(0, INT_VALUE);

	exchange.process_messages();

	ASSERT_EQ(1, value);
	ASSERT_EQ(1, INT_VALUE);

	INT_VALUE = 0;
}

TEST(TypeExchange, publish_multiple)
{
	TypeExchange exchange;

	int value {0};

	exchange.subscribe<int>([&value](int const& message) { value += message; });

	exchange.publish(1);
	exchange.publish(2);

	exchange.process_messages();

	ASSERT_EQ(3, value);
}

TEST(TypeExchange, subscribe_multiple)
{
	TypeExchange exchange;

	int value {0};

	exchange.subscribe<int>([&value](int const& message) { value += message; });
	exchange.subscribe<int>([&value](int const& message) { value += message; });

	exchange.publish(1);

	exchange.process_messages();

	ASSERT_EQ(2, value);
}

TEST(TypeExchange, subscribe_multiple_types)
{
	TypeExchange exchange;

	int int_value {0};
	std::string string_value;

	exchange.subscribe<int>([&int_value](int const& message) { int_value += message; });
	exchange.subscribe<std::string>([&string_value](std::string const& message) { string_value = message; });

	exchange.publish(1);
	exchange.publish(std::string {"Hello, world!"});

	exchange.process_messages();

	ASSERT_EQ(1, int_value);
	ASSERT_EQ("Hello, world!", string_value);
}

TEST(TypeExchange, non_copyable_type)
{
	TypeExchange exchange;

	NonCopyable nc;

	exchange.subscribe<NonCopyable>([](NonCopyable const& message) { (void) message; });

	exchange.publish(std::move(nc));
}

TEST(TypeExchange, non_moveable_type)
{
	TypeExchange exchange;

	NonMoveable nm;

	exchange.subscribe<NonMoveable>([](NonMoveable const& message) { (void) message; });

	exchange.publish(NonMoveable(nm));
}

TEST(TypeExchange, publish_then_subscribe)
{
	TypeExchange exchange;

	int value {0};

	exchange.publish(1);

	exchange.subscribe<int>([&value](int const& message) { value = message; });

	exchange.process_messages();

	ASSERT_EQ(1, value);
}

TEST(TypeExchange, receiver)
{
	TypeExchange exchange;

	Receiver receiver;

	exchange.subscribe<int>([&receiver](int const& message) { receiver.receive(message); });
	exchange.subscribe<std::string>([&receiver](std::string const& message) { receiver.receive(message); });

	exchange.publish(1);
	exchange.publish(2);
	exchange.publish(std::string {"Hello, "});
	exchange.publish(std::string {"world!"});

	exchange.process_messages();

	ASSERT_EQ(3, receiver._value);
	ASSERT_EQ("Hello, world!", receiver._str);
}
