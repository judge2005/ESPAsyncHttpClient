#ifndef ASYNCHTTPCLIENT_H_
#define ASYNCHTTPCLIENT_H_
#include "Arduino.h"
#ifdef ESP8266
#include "ESPAsyncTCP.h"
#else
#include "AsyncTCP.h"
#endif
#include <base64.h>

#ifndef DEBUG
#define DEBUG(...) {}
#endif


class ByteString: public String {
public:
	ByteString(void *data, size_t len) :
			String() {
		copy(data, len);
	}

	ByteString() :
			String() {
	}

	String& copy(const void *data, unsigned int length);
};

/**
 * Asynchronous HTTP Client
 */
struct AsyncHTTPClient {
	AsyncClient *aClient = NULL;

	bool initialized = false;
	String protocol;
	String base64Authorization;
	String host;
	int port;
	String uri;
	String request;

	ByteString response;
	int statusCode;
	void (*onSuccess)();
	void (*onFail)(String);

	void initialize(String url);
	int getStatusCode() { return statusCode; }

	String getBody();

	static void clientError(void *arg, AsyncClient *client, err_t error);
	static void clientDisconnect(void *arg, AsyncClient *client);
	static void clientData(void *arg, AsyncClient *client, void *data, size_t len);
	static void clientConnect(void *arg, AsyncClient *client);
	void makeRequest(void (*success)(), void (*fail)(String msg));
};

#endif ASYNCHTTPCLIENT_H_
