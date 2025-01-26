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
using MessageQueue = std::queue<std::unique_ptr<M>>;

template <typename M>
using SubscriberList = std::vector<MessageCallback<M>>;

class EventHandler
{
public:
	virtual ~EventHandler() = 0;

	virtual void process_messages() = 0;

	template <typename M>
	void subscribe(MessageCallback<M>&& callback);

	template <typename M>
	void publish(std::unique_ptr<M>&& message);

private:
	virtual auto subscriber_list_ptr() -> void* = 0;
	virtual auto message_queue_ptr() -> void* = 0;
};

inline EventHandler::~EventHandler() = default;

template <typename M>
void EventHandler::subscribe(MessageCallback<M>&& callback)
{
	auto& list = *static_cast<SubscriberList<M>*>(subscriber_list_ptr());

	list.push_back(std::move(callback));
}

template <typename M>
void EventHandler::publish(std::unique_ptr<M>&& message)
{
	auto& queue = *static_cast<MessageQueue<M>*>(message_queue_ptr());

	queue.push(std::move(message));
}

template <typename M>
class EventHandlerImpl : public EventHandler
{
public:
	~EventHandlerImpl() override = default;

	void process_messages() override;

private:
	auto subscriber_list_ptr() -> void* override;
	auto message_queue_ptr() -> void* override;

	SubscriberList<M> _subscribers;
	MessageQueue<M> _messages;
};

template <typename M>
void EventHandlerImpl<M>::process_messages()
{
	while (!_messages.empty()) {
		auto const& message = *_messages.front();

		for (auto const& callback : _subscribers) {
			callback(message);
		}

		_messages.pop();
	}
}

template <typename M>
auto EventHandlerImpl<M>::subscriber_list_ptr() -> void*
{
	return &_subscribers;
}

template <typename M>
auto EventHandlerImpl<M>::message_queue_ptr() -> void*
{
	return &_messages;
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
	for (auto& [type, handler] : _type_handlers) {
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
