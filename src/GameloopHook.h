#ifndef _GAMELOOP_HOOK_H_
#define _GAMELOOP_HOOK_H_

#include <SRHook/SRHook.hpp>

using GameloopPrototype = void( __cdecl* )();

class GameloopHook
{
	static GameloopHook* self;

	SRHook::Hook<> gameloopHook_{ 0x748DA3 };

	GameloopHook();
	~GameloopHook();
public:
	static void Initialize();
	static void UnInitialize();
private:
	void GameloopHooked( SRHook::CPU& CPU );
}; // class GameloopHook

#endif // !_GAMELOOP_HOOK_H_