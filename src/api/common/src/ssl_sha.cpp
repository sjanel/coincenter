#include "ssl_sha.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#include <openssl/sha.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>

#include "cct_exception.hpp"
#include "char-hexadecimal-converter.hpp"

namespace cct::ssl {
namespace {

constexpr auto ShaDigestLen(ShaType shaType) { return static_cast<unsigned int>(shaType); }

const EVP_MD* GetEVP_MD(ShaType shaType) { return shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(); }
}  // namespace

Md256 Sha256(std::string_view data) {
  static_assert(SHA256_DIGEST_LENGTH == static_cast<unsigned int>(ShaType::kSha256));

  Md256 ret;

  SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         reinterpret_cast<unsigned char*>(ret.data()));

  return ret;
}

std::string_view GetOpenSSLVersion() {
  static constexpr std::string_view kOpenSSLVersion = OPENSSL_VERSION_TEXT;
  return kOpenSSLVersion;
}

namespace {

template <ShaType shaType>
auto ShaBin(std::string_view data, std::string_view secret) {
  static constexpr unsigned int kExpectedLen = ShaDigestLen(shaType);

  unsigned int len = kExpectedLen;
  std::array<char, kExpectedLen> binData;

  HMAC(GetEVP_MD(shaType), secret.data(), static_cast<int>(secret.size()),
       reinterpret_cast<const unsigned char*>(data.data()), data.size(),
       reinterpret_cast<unsigned char*>(binData.data()), &len);

  if (len != kExpectedLen) {
    throw exception("Unexpected result from HMAC: expected len {}, got {}", kExpectedLen, len);
  }

  return binData;
}

template <std::size_t N>
std::array<char, 2UL * N> BinToLowerHex(const std::array<char, N>& binData) {
  std::array<char, 2UL * N> ret;

  auto out = ret.data();

  for (auto beg = binData.begin(), end = binData.end(); beg != end; ++beg) {
    out = to_lower_hex(*beg, out);
  }
  return ret;
}

template <ShaType shaType>
auto ShaHex(std::string_view data, std::string_view secret) {
  return BinToLowerHex(ShaBin<shaType>(data, secret));
}

}  // namespace

Md256 Sha256Bin(std::string_view data, std::string_view secret) { return ShaBin<ShaType::kSha256>(data, secret); }

Md512 Sha512Bin(std::string_view data, std::string_view secret) { return ShaBin<ShaType::kSha512>(data, secret); }

Sha256HexArray Sha256Hex(std::string_view data, std::string_view secret) {
  return ShaHex<ShaType::kSha256>(data, secret);
}

Sha512HexArray Sha512Hex(std::string_view data, std::string_view secret) {
  return ShaHex<ShaType::kSha512>(data, secret);
}

namespace {
using EVP_MD_CTX_UniquePtr = std::unique_ptr<EVP_MD_CTX, decltype([](EVP_MD_CTX* ptr) { EVP_MD_CTX_free(ptr); })>;

EVP_MD_CTX_UniquePtr CreateEVP_MD_CTX_UniquePtr(ShaType shaType) {
  EVP_MD_CTX_UniquePtr mdCtx(EVP_MD_CTX_new());

  EVP_DigestInit_ex(mdCtx.get(), GetEVP_MD(shaType), nullptr);

  return mdCtx;
}

template <ShaType shaType>
auto EVPBinToHex(const EVP_MD_CTX_UniquePtr& mdCtx) {
  static constexpr unsigned int kExpectedLen = ShaDigestLen(shaType);

  unsigned int len = kExpectedLen;
  std::array<char, kExpectedLen> binData;

  EVP_DigestFinal_ex(mdCtx.get(), reinterpret_cast<unsigned char*>(binData.data()), &len);

  if (len != kExpectedLen) {
    throw exception("Unexpected result from EVP_DigestFinal_ex: expected len {}, got {}", kExpectedLen, len);
  }

  return BinToLowerHex(binData);
}

template <ShaType shaType>
auto ShaDigest(std::string_view data) {
  auto mdCtx = CreateEVP_MD_CTX_UniquePtr(shaType);

  EVP_DigestUpdate(mdCtx.get(), data.data(), data.size());

  return EVPBinToHex<shaType>(mdCtx);
}

template <ShaType shaType>
auto ShaDigest(std::span<const std::string_view> data) {
  auto mdCtx = CreateEVP_MD_CTX_UniquePtr(shaType);

  std::ranges::for_each(data, [&](std::string_view str) { EVP_DigestUpdate(mdCtx.get(), str.data(), str.size()); });

  return EVPBinToHex<shaType>(mdCtx);
}
}  // namespace

Sha256DigestArray Sha256Digest(std::string_view data) { return ShaDigest<ShaType::kSha256>(data); }

Sha512DigestArray Sha512Digest(std::string_view data) { return ShaDigest<ShaType::kSha512>(data); }

Sha256DigestArray Sha256Digest(std::span<const std::string_view> data) { return ShaDigest<ShaType::kSha256>(data); }

Sha512DigestArray Sha512Digest(std::span<const std::string_view> data) { return ShaDigest<ShaType::kSha512>(data); }

}  // namespace cct::ssl
