#pragma once



#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

#include "Encryption/EncryptionStrategy.h"

namespace EasyIPC
{
	// Forward declare since users of this lib arent supposed to deal with nanomsg
	class NngSocket;

	class Server
	{
	public:
		Server();
		~Server();

		// Start the server at the given url and port
		// For simple local inter process communication use: tcp://localhost as url and any free port
		// You HAVE to explicitly call this before clients can connect.
		void serve(const std::string& url, uint16_t port);

		// Manually shutdown the server
		// Note: This also gets called in destructor
		void shutdown();

		/*
		Handle incoming events from connected clients
		Your handler should always accept a single nlohmann::json data argument,
		which represents the incoming parsed data.
		The handler can optionally return a response to the client
		who emitted the event by simply returning a nlohmann::json object.
		If you dont want to respond, simply dont return anything.

		1) Example handler that does respond to the client which emitted the event:

		NngIpcServer server;

		server.on("greet", [](const nlohmann::json& data)
		{
			std::cout << "Got greet event with data: " << data.dump(4) << "\n";

			// This will get sent back to the client who emitted "greet"
			return nlohmann::json{ {"number", 10} };
		});

		2) Example handler that does NOT respond to the client which emitted the event:

		server.on("greet", [](const nlohmann::json& data)
		{
			doSomething(data["someProperty"]);
		});
		*/
		template<typename HandlerType>
		void on(const std::string& event, HandlerType handler);

		// Emit an event to ALL connected clients with optional data (json object)
		void emit(const std::string& event, const nlohmann::json& data = {});

		// If you're using an encryption strategy and the communication is deemed compromised (e.g. message authentication code doesnt match)
		// then this callback will be invoked, e.g. when someone is trying to tamper with the traffic, think Wireshark.
		// The EncryptionStrategy subclass will need to support this feature, the provided AesEaxEncryptionStrategy does support this.
		// If you're using your own encryption strategy you will have to manually implement this, take a look at the existing strategy.
		void setOnCompromisedCallback(const std::function<void()>& callback);

		void setEncryptionStrategy(std::shared_ptr<EncryptionStrategy> strategy);

	private:
		void receiveLoop();
		void handleRequest(const std::string& message);

		std::unique_ptr<NngSocket> pubSocket;
		std::unique_ptr<NngSocket> repSocket;

		std::shared_ptr<EncryptionStrategy> encryptionStrategy;

		// Map events to callbacks that get passed the message which is already parsed to json object
		// Each handler can *optionally* return a response directly to the client who sent the message
		// by simply returning from the handler. For handlers that don't need to respond simply dont return anything.
		std::unordered_map<std::string, std::function<std::optional<nlohmann::json>(const nlohmann::json&)>> eventHandlers;
		std::mutex handlerMutex;

		std::thread receiveThread;
		std::atomic<bool> isRunning;
		std::atomic<bool> isStarted;

		std::string url;

		std::mutex shutdownMutex;
	};


	template<typename HandlerType>
	void Server::on(const std::string& event, HandlerType handler)
	{
		// get the return type of the passed lambda function
		using ReturnType = std::invoke_result_t<HandlerType, const nlohmann::json&>;

		// idea here is to wrap the handler and always returning a optional json response on paper
		// for handlers where the user doesnt return anything we return the null optional
		auto wrappedHandler = [handler](const nlohmann::json& data) -> std::optional<nlohmann::json>
		{

			// figure out at compile time if the handler is a void
			if constexpr (std::is_same_v<ReturnType, void>)
			{
				// if so just call it and return "nothing" for the std::optional
				handler(data);
				return std::nullopt;
			}
			else
			{
				// otherwise return whatever the user lambda returned
				return handler(data);
			}
		};

		std::lock_guard<std::mutex> lock(handlerMutex);

		// thanks to above approach all handlers follow same signature
		// but when using this code they dont need to care about any of this
		eventHandlers[event] = wrappedHandler;
	}
}


