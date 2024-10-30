![banner](https://i.imgur.com/YCsghuF.png)  
# EasyIPC - Inter Process Communication for CPP

**EasyIPC is a small inter process communication (IPC) library for C++.**\
**If you want to communicate between two processes in an easy event driven way using JSON, this library is for you.**

## Outline

1. [Conceptual Overview](#Conceptual-overview)
2. [.emit() & .on()](#.emit()-&-.on())
3. [Simple example](#Example-usage)
4. [Built-in encryption with message authentication](#Encryption-and-message-authentication)
5. [Installation](#Installation)

## Conceptual overview  

Conceptually this library distinguishes between **Servers** and **Clients**.  
A server may emit events to all connected clients and also handle events that clients send to it.  
A client may connect to *one* server and attach eventhandlers to all server emitted events its interested in  
and it may also emit events to the *one* server it is connected to.  

Both the server and the client are event emitters and event listeners.  
The difference between emitting an event from the server and emitting it from the client  
is that the server is emitting it to all connected clients, which could be more than one.  
The client instead always emits to the server it is connected to.  

In order to handle an emitted event you need to bind an event listener to that event on the other side.  
This closely resembles the way NodeJS handles event code.  

If you have never used NodeJS or JavaScript, this might be a bit unfamiliar.  
In short .emit() is used to send a specific "event" with data  
and .on() is used to react to a specific "event" and accept the data.  
In both cases the event is simply a string that can be anything you like  
that describes the event that is happening and the data is nlohmann::json.  


## .emit() & .on()
### To emit (send) events with data attached to the other side, use .emit() 

**Server emits to ALL connected clients:**
```cpp
EasyIPC::Server server{};
server.serve("tcp://localhost", PORT);
server.emit("important-event", {{"temperature", 98.5}});
```

**Client emits to the server its connected to:**
```cpp
EasyIPC::Client client{};
client.connect("tcp://localhost", PORT);

// Notice that when then client emits, the server can immediately respond
// to this specific client for convenience.
nlohmann::json serverResponse = client.emit("telemetry", {{"userAction",
  {"type", "mouseclick"},
  {"pos", {752, 126}}
}});
```


### To handle events from the other side, use .on() 

**Server handling the "telemetry" event from the above example:**
```cpp
EasyIPC::Server server{};
server.on("telemetry", [](const nlohmann::json& data)
{
    std::cout << "Useraction: " << data["userAction"]["type"] << " at " << data["userAction"]["pos"] << "\n";

    // For convenience: Returning from this handler callback will send this json back to the
    // original client who send the greet event
    return nlohmann::json{ {"success", true} };
});
```

**Client handling the "important-event" from the emit-example above:**
```cpp
EasyIPC::Client client{};
client.on("important-event", [](const nlohmann::json& data)
{
  // Easily access properties thanks to it being JSON
  int importantValue = data["temperature"];

  // Do something with data...
  std::cout << "New important value from server: " << importantValue << "\n";
});
```

## Example usage

This is a rather small example just to give you an overview how the usage looks like.  
For a full example that is ready to be build and played around with, check this repo:
[EasyIPC Example Repo](https://github.com/veteri/EasyIPC-example)

**Process A (Server)**  

```cpp
EasyIPC::Server server{};
const int PORT = 57239;

server.on("greet", [](const nlohmann::json& data)
{
    std::cout << "Got greet event with data: " << data.dump(4) << "\n";

    // For convenience: Returning from this handler callback will send this json back to the
    // original client who send the greet event
    return nlohmann::json{ {"someData", 10} };
});

server.serve("tcp://localhost", PORT);

while (true)
{
    Sleep(1000);
    // Send stuff to all connected clients
    server.emit("current-tick", {{"tick", GetTickCount()}};
}
```

**Process B (Client)**

```cpp
EasyIPC::Client client{};
const int PORT = 57239;

client.on("current-tick", [](const nlohmann::json& data)
{
    // Easily access properties thanks to it being JSON
    int tick = data["tick"];

    // Do something with data...
    std::cout << "New important value from server: " << tick << "\n";
});

client.connect("tcp://localhost", PORT);

nlohmann::json response = client.emit("greet", {{"someProp", "some string"}});
std::cout << response["someData"] << "\n"; // Prints: 10
```
## Encryption and message authentication

EasyIPC's server and client class have means to encrypt and decrypt the traffic.  
This is done using the strategy pattern, both the client and server class have a field with the type EncryptionStrategy  
which if set will be used to encrypt and decrypt the traffic.
The base class EncryptionStrategy has two pure virtual methods: `encrypt` and `decrypt`.
In order to make the server and client use your own encryption scheme, you'll have to 
make your own class that derives from the base class `EncryptionStrategy` and implement those two methods.  

If you want encryption but dont want to bother writing your own encrypt/decrypt methods,
the library comes with a provided encryption strategy using AES EAX mode which provides confidentiality AND authentication.  
To make use of the encryption simply include the strategy and set it on both client and server using
the method `void setEncryptionStrategy(std::shared_ptr<EncryptionStrategy> strategy);`

```cpp

// Server project:

#include "EasyIPC/Server.h"
#include "EasyIPC/Encryption/AesEaxEncryptionStrategy.h"

int main()
{
    EasyIPC::Server server;
    // You will have to make sure you use secure means and not store it in plain text in your code
    std::string aesKeyHex = "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4";
    server.setEncryptionStrategy(std::make_shared<EasyIPC::AesEaxEncryptionStrategy>(aesKeyHex));

    // will be transmitted encrypted
    server.emit("hi", {{"day", getDay()}});

    return 0;
}

// Client project:

#include "EasyIPC/Server.h"
#include "EasyIPC/Encryption/AesEaxEncryptionStrategy.h"

int main()
{
    EasyIPC::Client client;
    // You will have to make sure you use secure means and not store it in plain text in your code
    std::string aesKeyHex = "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4";
    client.setEncryptionStrategy(std::make_shared<EasyIPC::AesEaxEncryptionStrategy>(aesKeyHex));

    // will be transmitted encrypted
    client.on("hi", [](const nlohmann::json& data)
    {
        // Data is decrypted and preparsed already
        std::string day = data["day"];

        std::cout << "Today is: " << day << "\n";
    });

    return 0;
}
```

## Installation
This library is built ontop of nng (nanomsg-next-gen) and the built-in AES EAX encryption is  
built with cryptopp, the data of events is done through JSON (with nlohmann-json) for convenience.    
As such you will have to link against the following libraries:  

1. [nng](https://github.com/nanomsg/nng)
2. [cryptopp](https://github.com/weidai11/cryptopp)
3. [nlohmann-json](https://github.com/nlohmann/json)

### Installation of dependencies using vcpkg
While you could build all those dependencies from source, i recommend using vcpkg.  
So if you havent installed [vcpkg](https://github.com/microsoft/vcpkg) yet, then install it before proceeding with these instructions.  
There is two ways you can go about installing the dependencies (nng, cryptopp etc.) through vcpkg, either global or through manifest mode.  

I am only going to cover manifest mode in this installation guide.  
If you're having trouble with any step, you can also refer to this example repo  [EasyIPC Example Repo](https://github.com/veteri/EasyIPC-example)  
that has all these steps already completed. So if you're confused where to put a file  
looking at the structure or settings of the VS projects in that repo could help you out.  
However this shouldn't be necessary.

#### Installation guide using vcpkg manifest mode

1. Download the latest release of EasyIPC from here  
2. Extract the EasyIPC folder into your VS solution directory.  
   Since extracting usually creates another folder around the contents of the archive,  
   make sure to move the inner folder and not the outer.  
   Refer to the folder hierarchy in the [EasyIPC Example Repo](https://github.com/veteri/EasyIPC-example) repo, if you're not sure.
4. In your VS solution directory, create a new file called `vcpkg.json`
5. Paste the following json into that file.
```json
{
    "name": "name",
    "version": "1.0.0",
    "dependencies": [
        "nlohmann-json",
        "nng",
        "cryptopp"
    ]
}
``` 
3. You'll have to atleast use C++ 17 because the library uses std::optional.<br><br>
   ![Setting standard to latest](https://i.imgur.com/5spWrjZ.jpeg)

4. Make the project use vcpkg and specify that it uses manifest mode and that it uses static libraries.<br><br>
   ![Enabling vcpkg](https://i.imgur.com/GIyVhyW.jpeg)

5. Add EasyIPC include directory to the `Additional Include Directories`  
```
$(SolutionDir)EasyIPC/include
```
   
   ![includes](https://i.imgur.com/j8lAh0t.jpeg)

6. Add EasyIPC lib directory to the `Additional Library Directories`  
   Make sure you use the correct architecture x86 for 32bit and x64 for 64bit!  

```
$(SolutionDir)EasyIPC/lib/x64
```

   ![library directory](https://i.imgur.com/xscmVVl.jpeg)  

7. Add EasyIPC.lib as a dependency in the Linker Input

   **Release**<br>
   ![release dependency](https://i.imgur.com/o63dXPz.jpeg)

   **Debug**<br>
   ![debug dependency](https://i.imgur.com/YAeh93F.jpeg)

8. Set the `Runtime Library` in the C/C++ Code Generation to `MultiThreaded` (MT) for Release  
   and to `MultiThreaded Debug` (MTd) for Debug

    **Release**<br>
   ![runtimelib-release](https://i.imgur.com/5UJAuYT.jpeg)

   **Debug**<br>
   ![runtimelib-debug](https://i.imgur.com/G6aWE6h.jpeg)

9. Now EasyIPC should be ready to be used!
   If you have any issues, please first refer to the example repo and check for:
   1. Any difference folder hierarches
   2. Any typos in the include/library search paths
   3. Whether you used the wrong configuration, e.g. x86 vs x64 and Release vs Debug  

   Either way if you're stuck dont hesitate to open an issue.
