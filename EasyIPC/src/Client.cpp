#include "pch.h"
#include "Client.h"

#include <fmt/format.h>
#include <iostream>
#include <utility>

namespace EasyIPC
{
	Client::Client() :
		subSocket{},
		reqSocket{},
		isRunning{false},
		connected{false}
	{

	}

	Client::~Client()
	{
		shutdown();
	}

	void Client::connect(const std::string& url, uint16_t port, int maxRetries, int retryDelayMS)
	{
		int returnValue{};

		// first open pub/sub socket so we can receive events
		if ((returnValue = nng_sub0_open(&subSocket.get())) != 0)
		{
			throw std::runtime_error{ "Failed to open SUB socket: " + std::string(nng_strerror(returnValue)) };
		}

		subSocket.markOpen();

		// then open req socket for typical request/response type interactions
		if ((returnValue = nng_req0_open(&reqSocket.get())) != 0)
		{
			throw std::runtime_error{ "Failed to open REQ socket: " + std::string(nng_strerror(returnValue)) };
		}

		reqSocket.markOpen();

		std::string subSocketUrl = fmt::format("{}:{}", url, port);
		std::string reqSocketUrl = fmt::format("{}:{}", url, port + 1);

		// connect to server
		int attempts{ 0 };
		bool connectSuccess{ false };

		while (attempts < maxRetries)
		{
			returnValue = nng_dial(subSocket.get(), subSocketUrl.c_str(), nullptr, 0);
			if (returnValue == 0)
			{
				returnValue = nng_dial(reqSocket.get(), reqSocketUrl.c_str(), nullptr, 0);
				if (returnValue == 0)
				{
					connectSuccess = true;
					break;
				}
				else
				{
					lastReqDialError = nng_strerror(returnValue);
					std::cerr << "[Client::connect] Attempt" << (attempts + 1) << ": Failed to dial REQ socket: " << lastReqDialError << std::endl;
				}
			}
			else
			{
				lastSubDialError = nng_strerror(returnValue);
				std::cerr << "[Client::connect] Attempt" << (attempts + 1) << ": Failed to dial SUB socket: " << lastSubDialError << std::endl;
			}

			++attempts;
			if (attempts < maxRetries)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMS));
			}
		}

		if (!connectSuccess)
		{
			throw std::runtime_error(
				fmt::format(
					"Failed to connect to server after {} retries. Last sub socket dial error: {}, Last req socket dial error: {}",
					maxRetries,
					lastSubDialError,
					lastReqDialError
				)
			);
		}

		if ((returnValue = nng_setopt(subSocket.get(), NNG_OPT_SUB_SUBSCRIBE, "", 0)) != 0)
		{
			throw std::runtime_error{ "Failed to set subscribe option: " + std::string(nng_strerror(returnValue)) };
		}

		isRunning = true;
		receiveThread = std::thread(&Client::receiveLoop, this);
		connected = true;

		std::cout << "[EasyIPC::Client::connect] Started...\n";
	}

	bool Client::isConnected() const
	{
		return connected;
	}

	void Client::shutdown()
	{
		std::lock_guard<std::mutex> lock(shutdownMutex);

		if (isRunning)
		{
			isRunning = false;

			subSocket.close();
			reqSocket.close();

			if (receiveThread.joinable())
			{
				receiveThread.join();
			}

			connected = false;
		}

	}

	void Client::on(const std::string& event, std::function<void(const nlohmann::json&)> handler)
	{
		std::lock_guard<std::mutex> lock(handlerMutex);
		eventHandlers[event] = handler;
	}

	nlohmann::json Client::emit(const std::string& event, const nlohmann::json& data)
	{
		if (!connected)
		{
			throw std::runtime_error{ "Client is not connected. Cant emit." };
		}

		std::lock_guard<std::mutex> lock(reqMutex);

		nlohmann::json messageJson = {
			{"event", event},
			{"data", data}
		};

		std::string message = messageJson.dump(0);

		if (encryptionStrategy)
		{
			message = encryptionStrategy->encrypt(message);
		}

		int returnValue = nng_send(reqSocket.get(), message.data(), message.size(), 0);
		if (returnValue != 0)
		{
			throw std::runtime_error{ "Failed to send request: " + std::string(nng_strerror(returnValue)) };
		}

		char* buffer = nullptr;
		size_t size = 0;
		returnValue = nng_recv(reqSocket.get(), &buffer, &size, NNG_FLAG_ALLOC);
		if (returnValue != 0)
		{
			throw std::runtime_error{ "Failed to receive response: " + std::string(nng_strerror(returnValue)) };
		}

		std::string response(buffer, size);
		nng_free(buffer, size);

		if (encryptionStrategy)
		{
			response = encryptionStrategy->decrypt(response);
		}

		return nlohmann::json::parse(response);
	}

	void Client::setOnCompromisedCallback(const std::function<void()>& callback)
	{
		if (encryptionStrategy)
		{
			encryptionStrategy->setOnCompromisedHandler(callback);
		}
	}

	void Client::setEncryptionStrategy(std::shared_ptr<EncryptionStrategy> strategy)
	{
		encryptionStrategy = std::move(strategy);
	}

	void Client::receiveLoop()
	{
		std::cout << "[EasyIPC::Client::receiveLoop] Started...\n";

		while (isRunning)
		{
			char* buffer = nullptr;
			size_t size = 0;
			int returnValue = nng_recv(subSocket.get(), &buffer, &size, NNG_FLAG_ALLOC);
			if (returnValue == NNG_ECLOSED)
				break;

			if (returnValue != 0)
			{
				std::cerr << "Receive error: " << nng_strerror(returnValue) << std::endl;
				continue;
			}

			std::string message(buffer, size);
			nng_free(buffer, size);

			handleMessage(message);
		}
	}

	void Client::handleMessage(const std::string& message)
	{
		try
		{
			std::string plainMessage = message;

			if (encryptionStrategy)
			{
				plainMessage = encryptionStrategy->decrypt(message);
			}

			nlohmann::json messageJson = nlohmann::json::parse(plainMessage);
			std::string event = messageJson["event"];
			nlohmann::json data = messageJson["data"];

			std::lock_guard<std::mutex> lock(handlerMutex);
			if (eventHandlers.contains(event))
				eventHandlers[event](data);
			else
			{
				std::cerr << "[EasyIPC::Client::handleMessage] Unknown event: " << event << std::endl;
			}
		}
		catch (const std::exception& exception)
		{
			std::cerr << "[EasyIPC::Client::handleMessage] Exception: " << exception.what() << std::endl;
		}
	}

}