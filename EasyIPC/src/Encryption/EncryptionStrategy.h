#pragma once

#include <functional>
#include <string>

namespace EasyIPC
{
	class EncryptionStrategy
	{
	public:
		virtual ~EncryptionStrategy() = default;

		virtual std::string encrypt(const std::string& data) = 0;
		virtual std::string decrypt(const std::string& data) = 0;

		void setOnCompromisedHandler(const std::function<void()>& callback)
		{
			onCompromisedCallback = callback;
		}

	protected:
		std::function<void()> onCompromisedCallback{};
	};
}



