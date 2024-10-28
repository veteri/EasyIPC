#pragma once
#include "EncryptionStrategy.h"

namespace EasyIPC
{
	class NoEncryptionStrategy : public EncryptionStrategy
	{
	public:
		std::string encrypt(const std::string& data) override;
		std::string decrypt(const std::string& data) override;
	};
}


