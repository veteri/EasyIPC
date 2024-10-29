#include "pch.h"
#include "Server.h"

#include <iostream>

#include "NngSocket.h"

#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/reqrep0/rep.h>

namespace EasyIPC
{
	Server::Server() :
		pubSocket{ std::make_unique<NngSocket>() },
		repSocket{ std::make_unique<NngSocket>() },
		isRunning{ false },
		isStarted{ false }
	{

	}

	Server::~Server()
	{
		shutdown();
	}

	void Server::serve(const std::string& url, uint16_t port)
	{
		int returnValue{};

		if ((returnValue = nng_pub0_open(&pubSocket->get())) != 0)
		{
			throw std::runtime_error{ "Failed to open PUB socket: " + std::string(nng_strerror(returnValue)) };
		}

		pubSocket->markOpen();

		if ((returnValue = nng_rep0_open(&repSocket->get())) != 0)
		{
			throw std::runtime_error{ "Failed to open REP socket: " + std::string(nng_strerror(returnValue)) };
		}

		repSocket->markOpen();

		std::string pubSocketUrl = url + ":" + std::to_string(port);;
		std::string repSocketUrl = url + ":" + std::to_string(port + 1);;

		//std::cout << "publisher socket url: " << pubSocketUrl << "\n";
		//std::cout << "response socket url: " << repSocketUrl << "\n";

		if ((returnValue = nng_listen(pubSocket->get(), pubSocketUrl.c_str(), nullptr, 0)) != 0)
		{
			throw std::runtime_error{ "Failed to listen on PUB socket: " + std::string(nng_strerror(returnValue)) };
		}

		if ((returnValue = nng_listen(repSocket->get(), repSocketUrl.c_str(), nullptr, 0)) != 0)
		{
			throw std::runtime_error{ "Failed to listen on REP socket: " + std::string(nng_strerror(returnValue)) };
		}

		isRunning = true;
		receiveThread = std::thread(&Server::receiveLoop, this);
		isStarted = true;
		this->url = url;

		std::cout << "[EasyIPC::Server::connect] Started...\n";
	}

	void Server::shutdown()
	{
		std::lock_guard<std::mutex> lock(shutdownMutex);

		if (isRunning)
		{
			isRunning = false;
			pubSocket->close();
			repSocket->close();

			if (receiveThread.joinable())
			{
				receiveThread.join();
			}

			isStarted = false;
		}
	}

	void Server::emit(const std::string& event, const nlohmann::json& data)
	{
		if (!isStarted)
		{
			throw std::runtime_error{ "[EasyIPC::Server::emit] Server is not started" };
		}

		nlohmann::json messageJson = {
			{"event", event},
			{"data", data}
		};

		std::string message = messageJson.dump();

		if (encryptionStrategy)
		{
			message = encryptionStrategy->encrypt(message);
		}

		int returnValue = nng_send(pubSocket->get(), message.data(), message.size(), 0);
		if (returnValue != 0)
		{
			throw std::runtime_error{ "Failed to send message: " + std::string(nng_strerror(returnValue)) };
		}
	}

	void Server::setOnCompromisedCallback(const std::function<void()>& callback)
	{
		if (encryptionStrategy)
		{
			encryptionStrategy->setOnCompromisedHandler(callback);
		}
	}

	void Server::setEncryptionStrategy(std::shared_ptr<EncryptionStrategy> strategy)
	{
		encryptionStrategy = strategy;
	}

	void Server::receiveLoop()
	{
		while (isRunning)
		{
			void* buffer = nullptr;
			size_t size = 0;
			int returnValue = nng_recv(repSocket->get(), &buffer, &size, NNG_FLAG_ALLOC);

			if (returnValue == NNG_ECLOSED)
				break;
			if (returnValue != 0)
			{
				std::cerr << "[EasyIPC::Server::receiveLoop] Receive error: " << nng_strerror(returnValue) << "\n";
				continue;
			}

			std::string message(static_cast<char*>(buffer), size);
			nng_free(buffer, size);

			handleRequest(message);
		}
	}

	void Server::handleRequest(const std::string& message)
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

			std::optional<nlohmann::json> handlerResponse;

			{
				std::lock_guard<std::mutex> lock(handlerMutex);
				if (eventHandlers.contains(event))
				{
					handlerResponse = eventHandlers[event](data);
				}
				else
				{
					std::cerr << "[EasyIPC::Server::handleRequest] Received event " << event << " but no handler was bound for it.\n";
					std::string missingHandlerLabel = "Server has no handler bound for event: " + event;
					handlerResponse = {
						{"event", "__error__"},
						{"data", {
							{"message", missingHandlerLabel }
						}}
					};
				}
			}

			nlohmann::json responseJson;

			responseJson = handlerResponse.value_or(nlohmann::json{
				{"event", "__response__"},
				{"data", {
					{"status", "success"}
				}}
				});


			std::string response = responseJson.dump();

			if (encryptionStrategy)
			{
				response = encryptionStrategy->encrypt(response);
			}

			int returnValue = nng_send(repSocket->get(), response.data(), response.size(), 0);
			if (returnValue != 0)
			{
				std::cerr << "[EasyIPC::Server::handleRequest] Failed to send response: " << nng_strerror(returnValue) << "\n";
			}
		}
		catch (std::exception& exception)
		{
			std::cerr << "[EasyIPC::Server::handleRequest] Exception: " << exception.what() << "\n";

			nlohmann::json responseJson = {
			{"event", "__response__"},
			{"data",
				{
						{"status", "error"},
						{"message", exception.what()}
					}
			   }
			};

			std::string response = responseJson.dump();

			if (encryptionStrategy)
			{
				response = encryptionStrategy->encrypt(response);
			}

			int returnValue = nng_send(repSocket->get(), response.data(), response.size(), 0);
			if (returnValue != 0)
			{
				std::cerr << "[EasyIPC::Server::handleRequest] Failed to send response: " << nng_strerror(returnValue) << "\n";
			}
		}
	}
}