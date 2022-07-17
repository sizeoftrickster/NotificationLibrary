#include "TelegramNotifications.h"
#include "Utility.h"
#include <curl/mprintf.h>
#include <curl/multi.h>

TelegramNotifications::TelegramNotifications( CURLM* multihandle_ ) : MultiHandle( multihandle_ ) { }

void TelegramNotifications::sendMessage( std::string botToken, std::string chatId, std::string text, eParseMode parseMode, bool disableNotification, bool protectContent ) {
#pragma warning( push )
#pragma warning( disable : 26812)

	CURL* cURL = curl_easy_init();
	struct curl_slist* Headers{ nullptr };
	curl_mime* MIME{ nullptr };
	curl_mimepart* MIMEPart{ nullptr };
	char* URL = new char[128];

	if ( cURL ) {
		curl_msprintf( URL, "https://api.telegram.org/bot%s/%s", botToken.data(), "sendMessage" ); // Create API URL

		curl_easy_setopt( cURL, CURLOPT_POST, 1 ); // Request Method
		curl_easy_setopt( cURL, CURLOPT_URL, URL ); // Request URL
		curl_easy_setopt( cURL, CURLOPT_DEFAULT_PROTOCOL, "https" ); // Request Protocol

		curl_easy_setopt( cURL, CURLOPT_HTTPHEADER, Headers ); // Installing Headers

		curl_slist_append( Headers, "Content-Type: multipart/form-data" ); // Header -> Content-Type
		curl_slist_append( Headers, "charset=utf-8" ); // Header -> Charset

		MIME = curl_mime_init( cURL ); // Initialize MultipurposeInternetMailExtensions

		MIMEPart = curl_mime_addpart( MIME ); // First MIME's Part
		curl_mime_name( MIMEPart, "chat_id" );
		curl_mime_data( MIMEPart, chatId.c_str(), CURL_ZERO_TERMINATED );

		MIMEPart = curl_mime_addpart( MIME ); // Second MIME's Part
		curl_mime_name( MIMEPart, "text" );
		curl_mime_data( MIMEPart, Utility::win1251ToUTF8( text.c_str() ).c_str(), CURL_ZERO_TERMINATED );

		MIMEPart = curl_mime_addpart( MIME ); // Third MIME's Part
		curl_mime_name( MIMEPart, "parse_mode" );
		curl_mime_data( MIMEPart, GetNameOfParseMode( parseMode ).c_str(), CURL_ZERO_TERMINATED );

		if ( disableNotification ) {
			MIMEPart = curl_mime_addpart( MIME ); // Fourth MIME's Part
			curl_mime_name( MIMEPart, "disable_notification" );
			curl_mime_data( MIMEPart, "true", CURL_ZERO_TERMINATED );
		}

		if ( protectContent ) {
			MIMEPart = curl_mime_addpart( MIME ); // Fifth MIME's Part
			curl_mime_name( MIMEPart, "protect_content" );
			curl_mime_data( MIMEPart, "true", CURL_ZERO_TERMINATED );
		}

		curl_easy_setopt( cURL, CURLOPT_MIMEPOST, MIME ); // Install MIME

		curl_multi_add_handle( MultiHandle, cURL ); // Runing
	}

#pragma warning( pop )
}
void TelegramNotifications::sendMedia( eFileType fileType, std::string botToken, std::string chatId, std::string filePath, std::string caption, eParseMode parseMode, bool disableNotification, bool protectContent ) {
#pragma warning( push )
#pragma warning( disable : 26812)
	auto [telegramMethod, telegramArgument, MIMEType] = GetMediaInfo( fileType );
	CURL* cURL = curl_easy_init();
	struct curl_slist* Headers{ nullptr };
	curl_mime* MIME{ nullptr };
	curl_mimepart* MIMEPart{ nullptr };
	char* URL = new char[128];

	if ( cURL ) {
		curl_msprintf( URL, "https://api.telegram.org/bot%s/%s", botToken.data(), telegramMethod.data() ); // Create API URL

		curl_easy_setopt( cURL, CURLOPT_POST, 1 ); // Request Method
		curl_easy_setopt( cURL, CURLOPT_URL, URL ); // Request URL
		curl_easy_setopt( cURL, CURLOPT_DEFAULT_PROTOCOL, "https" ); // Request Protocol

		curl_easy_setopt( cURL, CURLOPT_HTTPHEADER, Headers ); // Installing Headers

		curl_slist_append( Headers, "Content-Type: multipart/form-data" ); // Header -> Content-Type
		curl_slist_append( Headers, "charset=utf-8" ); // Header -> Charset

		MIME = curl_mime_init( cURL ); // Initialize MultipurposeInternetMailExtensions
		
		MIMEPart = curl_mime_addpart( MIME ); // First MIME's Part
		curl_mime_name( MIMEPart, "chat_id" );
		curl_mime_data( MIMEPart, chatId.c_str(), CURL_ZERO_TERMINATED );

		MIMEPart = curl_mime_addpart( MIME ); // Second MIME's Part
		curl_mime_name( MIMEPart, telegramArgument.c_str() );
		curl_mime_filedata( MIMEPart, filePath.c_str() );
		curl_mime_type( MIMEPart, MIMEType.c_str() );

		MIMEPart = curl_mime_addpart( MIME ); // Third MIME's Part
		curl_mime_name( MIMEPart, "caption" );
		curl_mime_data( MIMEPart, Utility::win1251ToUTF8( caption.c_str() ).c_str(), CURL_ZERO_TERMINATED );

		MIMEPart = curl_mime_addpart( MIME ); // Fourth MIME's Part
		curl_mime_name( MIMEPart, "parse_mode" );
		curl_mime_data( MIMEPart, GetNameOfParseMode( parseMode ).c_str(), CURL_ZERO_TERMINATED );

		if ( disableNotification ) {
			MIMEPart = curl_mime_addpart( MIME ); // Fifth MIME's Part
			curl_mime_name( MIMEPart, "disable_notification" );
			curl_mime_data( MIMEPart, "true", CURL_ZERO_TERMINATED );
		}

		if ( protectContent ) {
			MIMEPart = curl_mime_addpart( MIME ); // Sixth MIME's Part
			curl_mime_name( MIMEPart, "protect_content" );
			curl_mime_data( MIMEPart, "true", CURL_ZERO_TERMINATED );
		}

		curl_easy_setopt( cURL, CURLOPT_MIMEPOST, MIME ); // Install MIME

		curl_multi_add_handle( MultiHandle, cURL ); // Runing

	}

#pragma warning( pop )
}

std::string TelegramNotifications::GetNameOfParseMode( eParseMode ParseMode ) {
	std::string result = "Nothing";
	switch ( ParseMode ) {
		case ( eParseMode::HTML ): {
			result = "HTML";
			break;
		}
		case ( eParseMode::MARKDOWN ): {
			result = "Markdown";
			break;
		}
		default: {
			result = "HTML";
			break;
		}
	}
	return result;
}
std::tuple<std::string, std::string, std::string> TelegramNotifications::GetMediaInfo( eFileType fileType ) {
	std::string telegramMethod = "sendPhoto";
	std::string telegramArgument = "photo";
	std::string MIMEType = "image";

	switch ( fileType ) {
		case ( eFileType::PHOTO ): {
			telegramMethod = "sendPhoto";
			telegramArgument = "photo";
			MIMEType = "image";
			break;
		}
		case ( eFileType::AUDIO ): {
			telegramMethod = "sendAudio";
			telegramArgument = "audio";
			MIMEType = "audio";
			break;
		}
		case ( eFileType::DOCUMENT ): {
			telegramMethod = "sendDocument";
			telegramArgument = "document";
			MIMEType = "application";
			break;
		}
		case ( eFileType::VIDEO ): {
			telegramMethod = "sendVideo";
			telegramArgument = "video";
			MIMEType = "video";
			break;
		}
		default: {
			telegramMethod = "sendPhoto";
			telegramArgument = "photo";
			MIMEType = "image";
			break;
		}
	}
	return std::make_tuple( telegramMethod, telegramArgument, MIMEType );
}