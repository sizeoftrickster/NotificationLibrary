#ifndef _ASYNC_REQUESTS_H_
#define _ASYNC_REQUESTS_H_

#include <curl/curl.h>
#include "DiscordNotifications.h"
#include "TelegramNotifications.h"

class AsyncRequests
{
	static AsyncRequests* self;

	CURLM* MultiHandle{ nullptr };
	int RunningHandles = 0;

	class TelegramNotifications* telegramNotf_{ nullptr };
	class DiscordNotifications* discordNotf_{ nullptr };

	AsyncRequests();
	~AsyncRequests();
public:
	static void Initialize();

	static TelegramNotifications* Telegram();
	static DiscordNotifications* Discord();

	static void MultiPerform();

	static void UnInitialize();
}; // class AsyncRequests

#endif // !_ASYNC_REQUESTS_H_