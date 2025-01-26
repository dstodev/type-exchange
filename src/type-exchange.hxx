#ifndef TYPE_EXCHANGE_HXX
#define TYPE_EXCHANGE_HXX

#include <functional>
#include <memory>
#include <queue>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace project {

template <typename M>
using MessageCallback = std::function<void(M const&)>;

namespace detail {

template <typename M>
using SubscriberList = std::vector<MessageCallback<M>>;

template <typename M>
using MessageQueue = std::queue<std::unique_ptr<M>>;

class EventHandler
{
public:
	virtual ~EventHandler() = 0;

	virtual void process_messages() = 0;
};

inline EventHandler::~EventHandler() = default;

template <typename M>
class EventHandlerImpl : public EventHandler
{
public:
	~EventHandlerImpl() override = default;

	void process_messages() override;

	void subscribe(MessageCallback<M>&& callback);
	void publish(std::unique_ptr<M>&& message);

private:
	SubscriberList<M> _subscribers;
	MessageQueue<M> _messages;
};

template <typename M>
void EventHandlerImpl<M>::process_messages()
{
	MessageQueue<M> messages;

	using std::swap;

	swap(messages, _messages);

	while (!messages.empty()) {
		auto const& message = *messages.front();

		for (auto const& subscriber : _subscribers) {
			subscriber(message);
		}

		messages.pop();
	}
}

template <typename M>
void EventHandlerImpl<M>::subscribe(MessageCallback<M>&& callback)
{
	_subscribers.push_back(std::move(callback));
}

template <typename M>
void EventHandlerImpl<M>::publish(std::unique_ptr<M>&& message)
{
	_messages.push(std::move(message));
}

}  // namespace detail

/** @Brief Facilitates arbitrary-type message transfer

    Allows subscribing to message types and publishing messages of those types
    to all subscribers.
 */
class TypeExchange
{
	// In a templated function, T&& parameters are a forwarding references.
	// Forwarding references bind to either lvalue or rvalue reference.
	// if_rvalue<M, R> restricts a function to only accept rvalue references.
	template <typename M, typename R>
	using if_rvalue = std::enable_if_t<std::is_rvalue_reference_v<M&&>, R>;

public:
	void process_messages();

	template <typename M>
	void subscribe(MessageCallback<M>&& callback);

	template <typename M>
	auto publish(M&& message) -> if_rvalue<M, void>;

private:
	template <typename M>
	auto get_handler() -> detail::EventHandlerImpl<M>&;

	using TypeHandlers = std::unordered_map<std::type_index, std::unique_ptr<detail::EventHandler>>;

	TypeHandlers _type_handlers;
};

inline void TypeExchange::process_messages()
{
	for (auto& [index, handler] : _type_handlers) {
		handler->process_messages();
	}
}

template <typename M>
void TypeExchange::subscribe(MessageCallback<M>&& callback)
{
	auto& handler = get_handler<M>();

	handler.subscribe(std::move(callback));
}

template <typename M>
auto TypeExchange::publish(M&& message) -> if_rvalue<M, void>
{
	using PushType = std::conditional_t<std::is_move_constructible_v<M>, M&&, M const&>;

	auto message_ptr = std::make_unique<M>(static_cast<PushType>(message));

	auto& handler = get_handler<M>();

	handler.publish(std::move(message_ptr));
}

template <typename M>
auto TypeExchange::get_handler() -> detail::EventHandlerImpl<M>&
{
	// operator[] creates a new handler for new indices
	auto& handler = _type_handlers[typeid(M)];

	using ImplType = std::remove_reference_t<decltype(get_handler<M>())>;

	// If a handler was just created, the instance pointer is still null
	if (!handler) {
		handler = std::make_unique<ImplType>();
	}

	// Return a reference to the handler instance
	return static_cast<ImplType&>(*handler);
}

}  // namespace project

#endif  // TYPE_EXCHANGE_HXX
