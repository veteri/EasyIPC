#pragma once
#include "EncryptionStrategy.h"

namespace EasyIPC
{
	class AesEaxEncryptionStrategy : public EncryptionStrategy
	{
	public:

		// Pass the key as a string for convenience (hex encoded)
		AesEaxEncryptionStrategy(const std::string& hexKey);

		std::string encrypt(const std::string& data) override;
		std::string decrypt(const std::string& data) override;

	private:

		std::vector<uint8_t> encryptionKey;

		void setKeyFromHexString(const std::string& hexKey);
	};
}


