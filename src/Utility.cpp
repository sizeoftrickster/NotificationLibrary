#include "Utility.h"
#include <Windows.h>

std::string Utility::win1251ToUTF8( const char* str ) {
    std::string res;
    int result_u, result_c;
    result_u = MultiByteToWideChar( 1251, 0, str, -1, 0, 0 );
    if ( !result_u ) { return 0; }
    wchar_t* ures = new wchar_t[result_u];
    if ( !MultiByteToWideChar( 1251, 0, str, -1, ures, result_u ) ) {
        delete[] ures;
        return 0;
    }
    result_c = WideCharToMultiByte( 65001, 0, ures, -1, 0, 0, 0, 0 );
    if ( !result_c ) {
        delete[] ures;
        return 0;
    }
    char* cres = new char[result_c];
    if ( !WideCharToMultiByte( 65001, 0, ures, -1, cres, result_c, 0, 0 ) ) {
        delete[] cres;
        return 0;
    }
    delete[] ures;
    res.append( cres );
    delete[] cres;
    return res;
}