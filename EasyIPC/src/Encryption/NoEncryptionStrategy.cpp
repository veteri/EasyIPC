#include "pch.h"
#include "NoEncryptionStrategy.h"

namespace EasyIPC
{
	std::string NoEncryptionStrategy::encrypt(const std::string& data)
	{
		return data;
	}

	std::string NoEncryptionStrategy::decrypt(const std::string& data)
	{
		return data;
	}
}