#ifndef TYPE_EXCHANGE_HXX
#define TYPE_EXCHANGE_HXX

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace project {

namespace detail {

struct Subscriber
{
	virtual ~Subscriber() = default;
	virtual void notify(void const* message) = 0;
};

template <typename MessageType, typename Callback>
struct SubscriberImpl : Subscriber
{
	explicit SubscriberImpl(Callback&& callback);

	void notify(void const* message) override;

	Callback _callback;
};

template <typename MessageType, typename Callback>
SubscriberImpl<MessageType, Callback>::SubscriberImpl(Callback&& callback)
    : _callback {std::forward<Callback>(callback)}
{}

template <typename MessageType, typename Callback>
void SubscriberImpl<MessageType, Callback>::notify(void const* message)
{
	_callback(*static_cast<MessageType const*>(message));
}

}  // namespace detail

/** @Brief Facilitates arbitrary-type message transfer

    Allows subscribing to message types and publishing messages of those types
    to all subscribers.
 */
class TypeExchange
{
public:
	template <typename MessageType, typename Callback>
	void subscribe(Callback&& callback);

	template <typename MessageType>
	void publish(MessageType const& message) const;

private:
	using TypeSubscribers = std::unordered_map<std::type_index, std::vector<std::unique_ptr<detail::Subscriber>>>;

	TypeSubscribers _type_subscribers;
};

template <typename MessageType, typename Callback>
void TypeExchange::subscribe(Callback&& callback)
{
	static_assert(std::is_invocable_v<Callback, MessageType const&>,
	              "Callback must be callable with a MessageType const& argument.");

	auto& subscribers = _type_subscribers[typeid(MessageType)];

	subscribers.emplace_back(
	    std::make_unique<detail::SubscriberImpl<MessageType, Callback>>(std::forward<Callback>(callback)));
}

template <typename MessageType>
void TypeExchange::publish(MessageType const& message) const
{
	auto const& subscribers = _type_subscribers.at(typeid(MessageType));

	for (auto const& subscriber : subscribers) {
		subscriber->notify(&message);
	}
}

}  // namespace project

#endif  // TYPE_EXCHANGE_HXX
