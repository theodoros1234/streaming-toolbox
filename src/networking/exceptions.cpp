#include "exceptions.h"
#include <string.h>

using namespace strtb::networking;

internal_error::internal_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
internal_error::internal_error(const std::string& what, int what_errno) : _what(what), _errno(what_errno) {}
internal_error::internal_error(int what_errno) : _what(strerror(what_errno)), _errno(what_errno) {}
const char* internal_error::what() const noexcept {return _what.c_str();}
int internal_error::what_errno() const noexcept {return _errno;}

address_resolution_error::address_resolution_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
address_resolution_error::address_resolution_error(const std::string& what, int what_errno) : _what(what), _errno(what_errno) {}
const char* address_resolution_error::what() const noexcept {return _what.c_str();}
int address_resolution_error::what_errno() const noexcept {return _errno;}

connection_error::connection_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
connection_error::connection_error(const std::string& what, int what_errno) : _what(what), _errno(what_errno) {}
connection_error::connection_error(int what_errno) : _what(strerror(what_errno)), _errno(what_errno) {}
const char* connection_error::what() const noexcept {return _what.c_str();}
int connection_error::what_errno() const noexcept {return _errno;}

connection_closed::connection_closed(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
connection_closed::connection_closed(const std::string& what, int what_errno) : _what(what), _errno(what_errno) {}
connection_closed::connection_closed(int what_errno) : _what(strerror(what_errno)), _errno(what_errno) {}
const char* connection_closed::what() const noexcept {return _what.c_str();}
int connection_closed::what_errno() const noexcept {return _errno;}

connection_error_ssl::connection_error_ssl(const char* what, int what_errno) : connection_error(what, what_errno) {}
connection_error_ssl::connection_error_ssl(const std::string& what, int what_errno) : connection_error(what, what_errno) {}
connection_error_ssl::connection_error_ssl(int what_errno) : connection_error(what_errno) {}

internal_error_ssl::internal_error_ssl(const char* what, int what_errno) : internal_error(what, what_errno) {}
internal_error_ssl::internal_error_ssl(const std::string& what, int what_errno) : internal_error(what, what_errno) {}
internal_error_ssl::internal_error_ssl(int what_errno) : internal_error(what_errno) {}

ssl_verification_error::ssl_verification_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
ssl_verification_error::ssl_verification_error(const std::string& what, int what_errno) : _what(what), _errno(what_errno) {}
const char* ssl_verification_error::what() const noexcept {return _what.c_str();}
int ssl_verification_error::what_errno() const noexcept {return _errno;}

bad_threading::bad_threading(const char* what) : _what(what) {}
bad_threading::bad_threading(const std::string& what) : _what(what) {}
const char* bad_threading::what() const noexcept {return _what.c_str();};
