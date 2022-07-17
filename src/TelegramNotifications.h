#ifndef _TELEGRAM_NOTIFICATIONS_H_
#define _TELEGRAM_NOTIFICATIONS_H_

#include <curl/curl.h>
#include <string>

class TelegramNotifications
{
	static constexpr int MAX_CHARACTER = 4096;

	CURLM* MultiHandle;
public:
	TelegramNotifications( CURLM* multihandle_ );
	~TelegramNotifications() {  };

	enum class eParseMode
	{
		HTML = 0,
		MARKDOWN = 1
	}; // enum class eParseMode
	enum class eFileType
	{
		PHOTO = 0,
		AUDIO = 1,
		DOCUMENT = 2,
		VIDEO = 3
	}; // enum class eFileType
	void sendMessage( std::string botToken, std::string chatId, std::string text, eParseMode parseMode, bool disableNotification, bool protectContent );
	void sendMedia( eFileType fileType, std::string botToken, std::string chatId, std::string filePath, std::string caption, eParseMode parseMode, bool disableNotification, bool protectContent );
private:
	std::string GetNameOfParseMode( eParseMode ParseMode );
	std::tuple<std::string, std::string, std::string> GetMediaInfo( eFileType fileType );
}; // class TelegramNotifications

#endif // !_TELEGRAM_NOTIFICATIONS_H_