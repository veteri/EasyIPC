#pragma once
#include <nng/nng.h>


namespace EasyIPC
{
	class NngSocket
	{
	public:
		~NngSocket();

		NngSocket();
		NngSocket(NngSocket&& other) noexcept;

		NngSocket& operator=(NngSocket&& other) noexcept;

		NngSocket(const NngSocket& other) = delete;
		NngSocket& operator=(const NngSocket& other) = delete;

		operator nng_socket& ();

		nng_socket& get() { return socket; }
		const nng_socket& get() const { return socket; }

		void markOpen();
		void close();

	private:
		nng_socket socket;
		bool isOpen;
	};
}

