#include <sol.hpp>
#include "GameloopHook.h"
#include "AsyncRequests.h"

void InitializeGameloopHook( sol::table& module ) {
	GameloopHook::Initialize();
	module.set_function( "UnHook", &GameloopHook::UnInitialize );
}

void InitializeCurl( sol::table& module ) {
	AsyncRequests::Initialize();
	module.set_function( "UnLoad", &AsyncRequests::UnInitialize );
}

void defineTelegramFunctions(sol::state_view& lua, sol::table& module) {
	lua.new_enum<TelegramNotifications::eParseMode>("TelegramParseMode", {
		{ "HTML", TelegramNotifications::eParseMode::HTML },
		{ "MARKDOWN", TelegramNotifications::eParseMode::MARKDOWN }
	});
	lua.new_enum<TelegramNotifications::eFileType>("TelegramFileType", {
		{ "PHOTO", TelegramNotifications::eFileType::PHOTO },
		{ "AUDIO", TelegramNotifications::eFileType::AUDIO },
		{ "DOCUMENT", TelegramNotifications::eFileType::DOCUMENT },
		{ "VIDEO", TelegramNotifications::eFileType::VIDEO }
	});
	module.set_function("sendTelegramMessage", []( sol::this_state ts, std::string botToken, std::string chatId, std::string text, TelegramNotifications::eParseMode parseMode = TelegramNotifications::eParseMode::HTML, bool disableNotification = false, bool protectContent = false ) {
		AsyncRequests::Telegram()->sendMessage( botToken, chatId, text, parseMode, disableNotification, protectContent );
	});
	module.set_function("sendTelegramMedia", []( sol::this_state ts, TelegramNotifications::eFileType fileType, std::string botToken, std::string chatId, std::string filePath, std::string caption, TelegramNotifications::eParseMode parseMode = TelegramNotifications::eParseMode::HTML, bool disableNotification = false, bool protectContent = false ) {
		AsyncRequests::Telegram()->sendMedia( fileType, botToken, chatId, filePath, caption, parseMode, disableNotification, protectContent );
	});
}

void defineDiscordFunctions( sol::table& module ) {
	module.set_function("sendDiscordMessage", []( sol::this_state ts, std::string webhookURL, std::string content, std::string username ) {
		AsyncRequests::Discord()->sendMessage( webhookURL, content, username );
	});
}

sol::table open( sol::this_state ThisState ) {
	sol::state_view lua( ThisState );
	sol::table module = lua.create_table();
	module["VERSION"] = 1.0;

	InitializeGameloopHook( module );
	InitializeCurl( module );
	
	defineTelegramFunctions( lua, module );
	defineDiscordFunctions( module );

	return module;
}

extern "C" __declspec(dllexport) int luaopen_NotificationLibraryDll( lua_State * LuaState ) {
	return ( sol::c_call<decltype( &( open ) ), &( open )> )( LuaState );
}

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD fdwReason, LPVOID lpReserved ) {
	switch ( fdwReason ) {
		case ( DLL_PROCESS_ATTACH ): {
			HMODULE hModule;
			GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>( &DllMain ), &hModule );
		}
	}
	return TRUE;
}
