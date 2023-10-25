#include "gethostname.hpp"

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#ifdef CCT_MSVC
#include <Winsock2.h>
#include <winsock.h>
#else
#include <unistd.h>

#include <cerrno>
#endif

namespace cct {
#ifdef CCT_MSVC
HostNameGetter::HostNameGetter() {
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(2, 2);
  auto errCode = WSAStartup(wVersionRequested, &wsaData);
  if (errCode != 0) {
    throw exception("Error {} in WSAStartup", errCode);
  }
}

HostNameGetter::~HostNameGetter() { WSACleanup(); }
#endif

// This method cannot be made static as it needs the RAII WSAStartup / WSACleanup wrapper in Windows.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
string HostNameGetter::getHostName() const {
  string hostname(16U, '\0');
  static constexpr string::size_type kMaxHostNameSize = 1024;
  string::size_type nullTerminatedCharPos = 0;
  do {
    auto errorCode = ::gethostname(hostname.data(), hostname.size() - 1U);
    if (errorCode != 0) {
#ifdef CCT_MSVC
      // In Windows, too small buffer returns an error WSAEFAULT
      if (WSAGetLastError() == WSAEFAULT) {
#else
      // In Posix, too small buffer returns an error ENAMETOOLONG set in errno
      if (errno == ENAMETOOLONG) {
#endif
        nullTerminatedCharPos = hostname.size() - 1U;
        hostname.resize(2 * hostname.size(), '\0');
        continue;
      }
      throw exception("Error {} in gethostname", errorCode);
    }
#ifndef CCT_MSVC
    if (hostname.back() != '\0') {
      // In POSIX, if the null-terminated hostname is too large to fit, then the name is truncated, and no error is
      // returned, meaning that last char has necessarily been written to
      nullTerminatedCharPos = hostname.size() - 1U;
      hostname.resize(2 * hostname.size(), '\0');
      continue;
    }
#endif
    if (hostname.size() > kMaxHostNameSize) {
      throw exception("Unexpected host name size length {}", hostname.size());
    }
    break;
  } while (true);

  auto hostnameSize = hostname.find('\0', nullTerminatedCharPos);
  if (hostnameSize == string::npos) {
    throw exception("Unexpected error in GetHostName algorithm");
  }
  hostname.resize(hostnameSize);
  return hostname;
}
}  // namespace cct