#pragma once

#include <mutex>
#include <thread>
#include <memory>


#include <nlohmann/json.hpp>

#include "Encryption/EncryptionStrategy.h"

namespace EasyIPC
{
	// Forward declare since users of this lib arent supposed to deal with nanomsg
	class NngSocket;

	class Client
	{
	public:

		Client();
		~Client();

		// For simple local inter process communication use: tcp://localhost as url and the port the server is listening on.
		// You HAVE to explicitly call this to connect to the server.
		void connect(const std::string& url, uint16_t port, int maxRetries = 5, int retryDelayMS = 1000);
		bool isConnected() const;

		void shutdown();

		// Attach event handler for specific event, but clients can't respond directly,
		// instead use .emit() to emit an event with optional data to the server that this client is connected to.
		// Conceptually clients only react to events and since many clients (processes) can react to an event
		// no one client can directly answer the message.
		// This method is equivalent to a subscribe in a publish/subscribe pattern.
		void on(const std::string& event, std::function<void(const nlohmann::json&)> handler);

		// Use to emit an event with optional json data to the server this client is connected to.
		// This is NOT the same as a publish method in the publish/subscribe pattern,
		// instead you only emit to the *single* server this client is connected to and not to any number of subscribers.
		// Note: This is a blocking call and will only return once the message has been delivered
		// to the server AND the server has responded.
		// The return value is the already parsed response from the server.
		nlohmann::json emit(const std::string& event, const nlohmann::json& data = {});

		std::string getLastSubSocketDialError() { return lastSubDialError; }
		std::string getLastReqSocketDialError() { return lastReqDialError; }

		// If you're using an encryption strategy and the communication is deemed compromised (e.g. message authentication code doesnt match)
		// then this callback will be invoked, e.g. when someone is trying to tamper with the traffic, think Wireshark.
		// The EncryptionStrategy subclass will need to support this feature, the provided AesEaxEncryptionStrategy does support this.
		// If you're using your own encryption strategy you will have to manually implement this, take a look at the existing strategy.
		void setOnCompromisedCallback(const std::function<void()>& callback);

		void setEncryptionStrategy(std::shared_ptr<EncryptionStrategy> strategy);

	private:

		void receiveLoop();
		void handleMessage(const std::string& message);

		std::unique_ptr<NngSocket> subSocket;
		std::unique_ptr<NngSocket> reqSocket;

		std::shared_ptr<EncryptionStrategy> encryptionStrategy;

		std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> eventHandlers;
		std::mutex handlerMutex;

		std::thread receiveThread;
		std::atomic<bool> isRunning;
		std::atomic<bool> connected;

		std::string connectUrl;
		std::string lastSubDialError{};
		std::string lastReqDialError{};

		std::mutex reqMutex;

		std::mutex shutdownMutex;
	};

}



