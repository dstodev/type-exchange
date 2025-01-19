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

namespace detail {

template <typename MessageType>
using MessageQueue = std::queue<MessageType>;

template <typename MessageType>
using MessageCallback = std::function<void(MessageType const&)>;

template <typename MessageType>
using SubscriberList = std::vector<MessageCallback<MessageType>>;

class EventHandler
{
public:
	virtual ~EventHandler() = 0;

	template <typename MessageType>
	auto subscriber_list_as_message_type() -> SubscriberList<MessageType>&;

	template <typename MessageType>
	auto message_queue_as_message_type() -> MessageQueue<MessageType>&;

	virtual void process_messages() = 0;

private:
	virtual auto subscriber_list_ptr() -> void* = 0;
	virtual auto message_queue_ptr() -> void* = 0;
};

inline EventHandler::~EventHandler() = default;

template <typename MessageType>
auto EventHandler::message_queue_as_message_type() -> MessageQueue<MessageType>&
{
	auto& queue = *static_cast<MessageQueue<MessageType>*>(message_queue_ptr());
	return queue;
}

template <typename MessageType>
auto EventHandler::subscriber_list_as_message_type() -> SubscriberList<MessageType>&
{
	auto& list = *static_cast<SubscriberList<MessageType>*>(subscriber_list_ptr());
	return list;
}

template <typename MessageType>
class EventHandlerImpl : public EventHandler
{
public:
	~EventHandlerImpl() override = default;

	void process_messages() override;

private:
	auto subscriber_list_ptr() -> void* override;
	auto message_queue_ptr() -> void* override;

	SubscriberList<MessageType> _callbacks;
	MessageQueue<MessageType> _messages;
};

template <typename MessageType>
auto EventHandlerImpl<MessageType>::subscriber_list_ptr() -> void*
{
	return &_callbacks;
}

template <typename MessageType>
auto EventHandlerImpl<MessageType>::message_queue_ptr() -> void*
{
	return &_messages;
}

template <typename MessageType>
void EventHandlerImpl<MessageType>::process_messages()
{
	while (!_messages.empty()) {
		auto const& message = _messages.front();

		for (auto const& callback : _callbacks) {
			callback(message);
		}

		_messages.pop();
	}
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
	void subscribe(Callback callback);

	template <typename MessageType>
	void publish(MessageType message);

	void process_messages();

private:
	template <typename MessageType>
	auto get_handler() -> detail::EventHandlerImpl<MessageType>&;

	using TypeHandlers = std::unordered_map<std::type_index, std::unique_ptr<detail::EventHandler>>;

	TypeHandlers _type_handlers;
};

template <typename MessageType, typename Callback>
void TypeExchange::subscribe(Callback callback)
{
	static_assert(std::is_invocable_v<Callback, MessageType const&>,
	              "Callback must be callable with a MessageType const& argument.");

	auto& handler = get_handler<MessageType>();
	auto& subscribers = handler.template subscriber_list_as_message_type<MessageType>();

	subscribers.push_back(std::move(callback));
}

template <typename MessageType>
void TypeExchange::publish(MessageType message)
{
	auto& handler = get_handler<MessageType>();
	auto& queue = handler.template message_queue_as_message_type<MessageType>();

	queue.push(std::move(message));
}

template <typename MessageType>
auto TypeExchange::get_handler() -> detail::EventHandlerImpl<MessageType>&
{
	auto& handler = _type_handlers[typeid(MessageType)];

	if (!handler) {
		handler = std::make_unique<detail::EventHandlerImpl<MessageType>>();
	}

	return static_cast<detail::EventHandlerImpl<MessageType>&>(*handler);
}

inline void TypeExchange::process_messages()
{
	for (auto& [type, handler] : _type_handlers) {
		handler->process_messages();
	}
}

}  // namespace project

#endif  // TYPE_EXCHANGE_HXX
