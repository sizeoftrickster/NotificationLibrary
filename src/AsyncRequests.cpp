#include "AsyncRequests.h"

AsyncRequests* AsyncRequests::self{ nullptr };

AsyncRequests::AsyncRequests() {
	if ( MultiHandle )
		return;
	MultiHandle = curl_multi_init();
}

AsyncRequests::~AsyncRequests() {
	if ( !MultiHandle )
		return;
	curl_multi_cleanup( MultiHandle );
}

void AsyncRequests::Initialize() {
	if ( !self )
		self = new AsyncRequests();
}

DiscordNotifications* AsyncRequests::Discord() {
	if ( self->MultiHandle != nullptr ) {
		if ( !self->discordNotf_ ) self->discordNotf_ = new DiscordNotifications( self->MultiHandle );
		return self->discordNotf_;
	}
	return nullptr;
}

TelegramNotifications* AsyncRequests::Telegram() {
	if ( self->MultiHandle != nullptr ) {
		if ( !self->telegramNotf_ ) self->telegramNotf_ = new TelegramNotifications( self->MultiHandle );
		return self->telegramNotf_;
	}
	return nullptr;
}

void AsyncRequests::MultiPerform() {
	if ( self->MultiHandle )
		curl_multi_perform( self->MultiHandle, &self->RunningHandles );
}

void AsyncRequests::UnInitialize() {
	if ( self ) {
		delete self;
		self = nullptr;
	}
}