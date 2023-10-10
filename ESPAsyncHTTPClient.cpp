#include <ESPAsyncHTTPClient.h>

#define DEBUG(...) {}

String& ByteString::copy(const void *data, unsigned int length) {
	if (!reserve(length)) {
		invalidate();
		return *this;
	}
#if defined (ESP32) || defined (NEW_WSTRING)
	setLen(length);
	memcpy(wbuffer(), data, length);
	wbuffer()[length] = 0;
#else
	len = length;
	memcpy(buffer, data, length);
	buffer[length] = 0;
#endif
	return *this;
}

void AsyncHTTPClient::initialize(String url) {
	// check for : (http: or https:
	int index = url.indexOf(':');
	if (index < 0) {
		initialized = false;	// This is not a URL
	}

	protocol = url.substring(0, index);
	DEBUG(protocol);
	port = 80;	//Default
	if (index == 5) {
		port = 443;
	}

	url.remove(0, (index + 3)); // remove http:// or https://

	index = url.indexOf('/');
	String hostPart = url.substring(0, index);
	DEBUG(hostPart);
	url.remove(0, index); // remove hostPart part

	// get Authorization
	index = hostPart.indexOf('@');

	if (index >= 0) {
		// auth info
		String auth = hostPart.substring(0, index);
		hostPart.remove(0, index + 1); // remove auth part including @
		base64Authorization = base64::encode(auth);
	}

	// get port
	index = hostPart.indexOf(':');
	if (index >= 0) {
		host = hostPart.substring(0, index); // hostname
		host.remove(0, (index + 1)); // remove hostname + :
		DEBUG(host);
		port = host.toInt(); // get port
		DEBUG(port);
	} else {
		host = hostPart;
		DEBUG(host);
	}
	uri = url;
#if	ASYNC_TCP_SSL_ENABLED
	initialized = protocol == "http" || protocol == "https";
#else
	initialized = protocol == "http";
#endif

	DEBUG(initialized);
	request = "GET " + uri + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";

	DEBUG(request);
	initialized = true;
}

String AsyncHTTPClient::getBody() {
	if (statusCode == 200) {
		// This is not a good check for chunked transfer encoding - it should ignore case and spaces
		int chunkedIndex = response.indexOf("Transfer-Encoding: chunked");
		int bodyStart = response.indexOf("\r\n\r\n") + 4;
		if (bodyStart <= 3) {
			return "";
		}
		if (chunkedIndex != -1) {
			String body;
			const char *buf = response.c_str();
			int chunkSize = strtol(buf + bodyStart, 0, 16);
			while (chunkSize) {
				// Move past chunk size
				bodyStart = response.indexOf("\r\n", bodyStart) + 2;
				body += response.substring(bodyStart, bodyStart + chunkSize);

				// Move past chunk
				bodyStart = response.indexOf("\r\n", bodyStart + chunkSize) + 2;
				chunkSize = strtol(buf + bodyStart, 0, 16);
			}

			return body;
		} else {
			return response.substring(bodyStart);
		}
	} else {
		return "";
	}
}

void AsyncHTTPClient::clientError(void *arg, AsyncClient *client,
		err_t error) {
	DEBUG("Connect Error");
	AsyncHTTPClient *self = (AsyncHTTPClient*) arg;
	self->onFail("Connection error");
	self->aClient = NULL;
	delete client;
}

void AsyncHTTPClient::clientDisconnect(void *arg, AsyncClient *client) {
	DEBUG("Disconnected");
	AsyncHTTPClient *self = (AsyncHTTPClient*) arg;
	self->aClient = NULL;
	delete client;
}

void AsyncHTTPClient::clientData(void *arg, AsyncClient *client, void *data, size_t len) {
	DEBUG("Got response");

	AsyncHTTPClient *self = (AsyncHTTPClient*) arg;
	self->response = ByteString(data, len);
	String status = self->response.substring(9, 12);
	self->statusCode = atoi(status.c_str());
	DEBUG(status.c_str());

	if (self->statusCode == 200) {
		self->onSuccess();
	} else {
		self->onFail("Failed with code " + status);
	}
}

void AsyncHTTPClient::clientConnect(void *arg, AsyncClient *client) {
	DEBUG("Connected");

	AsyncHTTPClient *self = (AsyncHTTPClient*) arg;

	self->response.copy("", 0);
	self->statusCode = -1;

	// Clear oneError handler
	self->aClient->onError(NULL, NULL);

	// Set disconnect handler
	client->onDisconnect(clientDisconnect, self);

	client->onData(clientData, self);

	//send the request
	client->write(self->request.c_str());
}

void AsyncHTTPClient::makeRequest(void (*success)(), void (*fail)(String msg)) {
	onFail = fail;

	if (!initialized) {
		fail("Not initialized");
		return;
	}

	if (aClient) { //client already exists
		fail("Call taking forever");
		return;
	}

	aClient = new AsyncClient();
	if (!aClient) { //could not allocate client
		fail("Out of memory");
		return;
	}

	onSuccess = success;

	aClient->onError(clientError, this);

	aClient->onConnect(clientConnect, this);
	bool connected = false;
#if	ASYNC_TCP_SSL_ENABLED
	if (protocol == "https") {
		DEBUG("Creating secure connection");
		connected = aClient->connect(host.c_str(), port, true);
	} else {
		connected = aClient->connect(host.c_str(), port, false);
	}
#else
	connected = aClient->connect(host.c_str(), port);
#endif
	if (!connected) {
		DEBUG("Connect Fail");
		fail("Connection failed");
		AsyncClient * client = aClient;
		aClient = NULL;
		delete client;
	}
}

