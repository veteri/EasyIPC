#include "pch.h"
#include "NngSocket.h"

namespace EasyIPC
{
	NngSocket::~NngSocket()
	{
		close();
	}

	NngSocket::NngSocket() :
		socket{ NNG_SOCKET_INITIALIZER },
		isOpen{ false }
	{

	}

	NngSocket::NngSocket(NngSocket&& other) noexcept :
		socket{ other.socket },
		isOpen{ other.isOpen }
	{
		other.socket = NNG_SOCKET_INITIALIZER;
		other.isOpen = false;
	}

	NngSocket& NngSocket::operator=(NngSocket&& other) noexcept
	{
		if (this != &other)
		{
			close();
			socket = other.socket;
			isOpen = other.isOpen;
			other.socket = NNG_SOCKET_INITIALIZER;
			other.isOpen = false;
		}

		return *this;
	}

	NngSocket::operator nng_socket_s& ()
	{
		return socket;
	}

	void NngSocket::markOpen()
	{
		isOpen = true;
	}

	void NngSocket::close()
	{
		if (isOpen)
		{
			nng_close(socket);
			isOpen = false;
		}
	}
}

