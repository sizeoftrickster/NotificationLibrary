#ifndef _DISCORD_NOTIFICATIONS_H_
#define _DISCORD_NOTIFICATIONS_H_

#include <curl/curl.h>
#include <string>

class DiscordNotifications
{
	static constexpr int MAX_CHARACTER = 2000;

	CURLM* MultiHandle;
public:
	DiscordNotifications( CURLM* mhandle );
	~DiscordNotifications() {};

	void sendMessage( std::string webhookURL, std::string content, std::string username );
}; // class DiscordNotifications

#endif // !_DISCORD_NOTIFICATIONS_H_