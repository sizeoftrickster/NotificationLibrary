#include "DiscordNotifications.h"
#include "Utility.h"
#include <curl/mprintf.h>

DiscordNotifications::DiscordNotifications( CURLM* multihandle_ ) : MultiHandle( multihandle_ ) { }

void DiscordNotifications::sendMessage( std::string webhookURL, std::string content, std::string username ) {
#pragma warning( push )
#pragma warning( disable : 26812)

	CURL* cURL = curl_easy_init();
	struct curl_slist* Headers{ nullptr };
	curl_mime* MIME{ nullptr };
	curl_mimepart* MIMEPart{ nullptr };

	if ( cURL ) {
		curl_easy_setopt( cURL, CURLOPT_POST, 1 ); // Request Method
		curl_easy_setopt( cURL, CURLOPT_URL, webhookURL.c_str() ); // Request URL
		curl_easy_setopt( cURL, CURLOPT_DEFAULT_PROTOCOL, "https" ); // Request Protocol

		curl_easy_setopt( cURL, CURLOPT_HTTPHEADER, Headers ); // Installing Headers

		curl_slist_append( Headers, "Content-Type: multipart/form-data" ); // Header -> Content-Type
		curl_slist_append( Headers, "charset=utf-8" ); // Header -> Charset

		MIME = curl_mime_init( cURL ); // Initialize MultipurposeInternetMailExtensions
		
		MIMEPart = curl_mime_addpart( MIME ); // First MIME's Part
		curl_mime_name( MIMEPart, "content" );
		curl_mime_data( MIMEPart, Utility::win1251ToUTF8( content.c_str() ).c_str(), CURL_ZERO_TERMINATED );

		MIMEPart = curl_mime_addpart( MIME ); // Second MIME's Part
		curl_mime_name( MIMEPart, "username" );
		curl_mime_data( MIMEPart, Utility::win1251ToUTF8( username.c_str() ).c_str(), CURL_ZERO_TERMINATED );

		curl_easy_setopt( cURL, CURLOPT_MIMEPOST, MIME ); // Install MIME

		curl_multi_add_handle( MultiHandle, cURL ); // Runing
	}

#pragma warning( pop )
}