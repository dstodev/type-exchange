#ifndef TYPE_EXCHANGE_HXX
#define TYPE_EXCHANGE_HXX

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace project {

namespace detail {

struct MessageCallWrapper
{
	virtual ~MessageCallWrapper() = default;
	virtual void call(void const* message) = 0;
};

template <typename MessageType, typename Callback>
struct MessageCallWrapperImpl : MessageCallWrapper
{
	explicit MessageCallWrapperImpl(Callback&& callback);

	void call(void const* message) override;

	Callback _callback;
};

template <typename MessageType, typename Callback>
MessageCallWrapperImpl<MessageType, Callback>::MessageCallWrapperImpl(Callback&& callback)
    : _callback {std::forward<Callback>(callback)}
{}

template <typename MessageType, typename Callback>
void MessageCallWrapperImpl<MessageType, Callback>::call(void const* message)
{
	_callback(*static_cast<MessageType const*>(message));
}

}  // namespace detail

/** @Brief Facilitates arbitrary-type message transfer

    Allows subscribing to message types and publishing messages of those types:

      subscribe<MessageType>([](MessageType const& message) { ... });
      publish<MessageType>(message);
 */
class TypeExchange
{
public:
	template <typename MessageType, typename Callback>
	void subscribe(Callback&& callback);

	template <typename MessageType>
	void publish(MessageType const& message) const;

private:
	using CallbackMap = std::unordered_map<std::type_index, std::vector<std::unique_ptr<detail::MessageCallWrapper>>>;

	CallbackMap _subscribers;
};

template <typename MessageType, typename Callback>
void TypeExchange::subscribe(Callback&& callback)
{
	static_assert(std::is_invocable_v<Callback, MessageType const&>,
	              "Callback must be callable with a MessageType const& argument.");

	auto& callbacks = _subscribers[typeid(MessageType)];

	callbacks.push_back(std::make_unique<detail::MessageCallWrapperImpl<MessageType, std::decay_t<Callback>>>(
	    std::forward<Callback>(callback)));
}

template <typename MessageType>
void TypeExchange::publish(MessageType const& message) const
{
	auto const& callbacks = _subscribers.at(typeid(MessageType));

	for (auto const& callback : callbacks) {
		callback->call(&message);
	}
}

}  // namespace project

#endif  // TYPE_EXCHANGE_HXX
