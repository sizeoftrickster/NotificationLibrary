#include "SRBaseHook.h"
#include <cstdarg>
#include <fstream>
#include <string>

#if defined( _WIN32 )
#	include <windows.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

void SRHook::dbg_out( bool crit, const char *fmt, ... ) {
	if ( !crit && !debug ) return;

	static std::mutex	 mtx;
	static char			 static_buffer[4096]{ 0 };
	static std::ofstream out;
	std::lock_guard		 lock( mtx );
	if ( !out.is_open() ) {
#if defined( _WIN32 )
		std::string module_path;
		module_path.resize( MAX_PATH, '\0' );
		GetModuleFileNameA( (HMODULE)&__ImageBase, (char *)module_path.data(), MAX_PATH );
		auto sep = module_path.rfind( "\\" );
		if ( sep == std::string::npos ) sep = module_path.rfind( "/" );
		auto module = module_path.substr( sep + 1 );
		out.open( "!SRHook_" + module + ".log" );
#else
		out.open( "!SRHook.log" );
#endif
	}

	if ( fmt == nullptr ) {
		out.flush();
		return;
	}

	va_list argptr;
	va_start( argptr, fmt );

	vsprintf( static_buffer, fmt, argptr );
	va_end( argptr );

	out << static_buffer << std::endl;
	out.flush();
}
