#include "GameloopHook.h"
#include "AsyncRequests.h"

GameloopHook* GameloopHook::self{ nullptr };

GameloopHook::GameloopHook() {
	gameloopHook_.onBefore += std::make_tuple( this, &GameloopHook::GameloopHooked );
	gameloopHook_.install();
}

GameloopHook::~GameloopHook() {
	gameloopHook_.remove();
}

void GameloopHook::Initialize() {
	if ( !self )
		self = new GameloopHook();
}

void GameloopHook::UnInitialize() {
	if ( self ) {
		delete self;
		self = nullptr;
	}
}

void GameloopHook::GameloopHooked( SRHook::CPU& CPU ) {
	AsyncRequests::MultiPerform();
}