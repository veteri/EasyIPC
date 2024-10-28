#include "pch.h"
#include "AesEaxEncryptionStrategy.h"

#include <cryptopp/aes.h>
#include <cryptopp/eax.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <stdexcept>
#include <fmt/format.h>

namespace EasyIPC
{
	AesEaxEncryptionStrategy::AesEaxEncryptionStrategy(const std::string& hexKey)
	{
		setKeyFromHexString(hexKey);
		size_t keyLength = encryptionKey.size();
		if (keyLength != 16 && keyLength != 24 && keyLength != 32)
		{
			throw std::invalid_argument{ fmt::format("Invalid key length {}", encryptionKey.size()) };
		}
	}

	std::string AesEaxEncryptionStrategy::encrypt(const std::string& data)
	{
		using namespace CryptoPP;

		AutoSeededRandomPool rng;
		byte nonce[16]{};

		rng.GenerateBlock(nonce, sizeof(nonce));

		std::string cipherText;
		EAX<AES>::Encryption encryptor;
		encryptor.SetKeyWithIV(encryptionKey.data(), encryptionKey.size(), nonce, sizeof(nonce));

		StringSource ss(data, true,
			new AuthenticatedEncryptionFilter(encryptor, new StringSink(cipherText))
		);

		std::string finalMessage(reinterpret_cast<char*>(nonce), sizeof(nonce));
		finalMessage += cipherText;

		return finalMessage;
	}

	std::string AesEaxEncryptionStrategy::decrypt(const std::string& data)
	{
		using namespace CryptoPP;

		if (data.size() < (16 + 16)) // nonce is 16 and minimum cipher 16
		{
			if (onCompromisedCallback)
				onCompromisedCallback();

			throw std::runtime_error{ "Invalid size" };
		}

		const byte* nonce = reinterpret_cast<const byte*>(data.data());
		size_t nonceSize = 16;

		const byte* cipherText = nonce + nonceSize;
		size_t cipherTextSize = data.size() - nonceSize;

		std::string plainText;
		try
		{
			EAX<AES>::Decryption decryptor;
			decryptor.SetKeyWithIV(encryptionKey.data(), encryptionKey.size(), nonce, nonceSize);

			StringSource ss(cipherText, cipherTextSize, true,
				new AuthenticatedDecryptionFilter(decryptor,
					new StringSink(plainText),
					AuthenticatedDecryptionFilter::THROW_EXCEPTION
				)
			);
		}
		catch (const CryptoPP::Exception& exception)
		{
			if (onCompromisedCallback)
				onCompromisedCallback();

			throw std::runtime_error("Decryption failed: " + std::string(exception.what()));
		}

		return plainText;
	}

	void AesEaxEncryptionStrategy::setKeyFromHexString(const std::string& hexKey)
	{
		using namespace CryptoPP;

		try
		{
			StringSource ss(hexKey, true,
				new HexDecoder(
					new VectorSink(encryptionKey)
				)
			);
		}
		catch (const CryptoPP::Exception& exception)
		{
			throw std::invalid_argument("Invalid hex: " + std::string(exception.what()));
		}
	}
}