// Wild Conquest - Shared Workspace Implementation (v2 Round-Trip Pipeline)
// Copyright (C) 2026 Wild Conquest Team

#include "app/wildconquest/wc_workspace.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <cstring>

#include "app/doc.h"
#include "app/context.h"
#include "app/cmd/add_slice.h"
#include "app/cmd/add_tag.h"
#include "app/file/file.h"
#include "app/tx.h"
#include "app/doc_api.h"
#include "doc/sprite.h"
#include "doc/layer.h"
#include "doc/image.h"
#include "doc/tag.h"
#include "doc/cel.h"
#include "doc/primitives.h"
#include "render/render.h"

#include "json11.hpp"
#include "png.h"

namespace fs = std::filesystem;

namespace wildconquest {

namespace {

// Self-contained, standard SHA-256 implementation
class Sha256 {
public:
  Sha256() { reset(); }

  void reset() {
    m_len = 0;
    m_h[0] = 0x6a09e667;
    m_h[1] = 0xbb67ae85;
    m_h[2] = 0x3c6ef372;
    m_h[3] = 0xa54ff53a;
    m_h[4] = 0x510e527f;
    m_h[5] = 0x9b05688c;
    m_h[6] = 0x1f83d9ab;
    m_h[7] = 0x5be0cd19;
  }

  void update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      m_buf[m_len % 64] = data[i];
      m_len++;
      if (m_len % 64 == 0) {
        transform(m_buf);
      }
    }
  }

  std::string finalize() {
    uint8_t pad[64] = {0};
    pad[0] = 0x80;
    size_t rem = m_len % 64;
    size_t padLen = (rem < 56) ? (56 - rem) : (120 - rem);
    update(pad, padLen);

    uint8_t bitLen[8];
    uint64_t totalBits = m_len * 8;
    for (int i = 0; i < 8; ++i) {
      bitLen[7 - i] = static_cast<uint8_t>(totalBits >> (i * 8));
    }
    update(bitLen, 8);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
      ss << std::hex << std::setw(8) << std::setfill('0') << m_h[i];
    }
    return ss.str();
  }

private:
  static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
  static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static inline uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static inline uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static inline uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static inline uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

  void transform(const uint8_t* chunk) {
    static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
             (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
             (static_cast<uint32_t>(chunk[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
      w[i] = sig1(w[i - 2]) + w[i - 7] + sig0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = m_h[0], b = m_h[1], c = m_h[2], d = m_h[3];
    uint32_t e = m_h[4], f = m_h[5], g = m_h[6], h = m_h[7];

    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = h + ep1(e) + ch(e, f, g) + K[i] + w[i];
      uint32_t t2 = ep0(a) + maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }

    m_h[0] += a;
    m_h[1] += b;
    m_h[2] += c;
    m_h[3] += d;
    m_h[4] += e;
    m_h[5] += f;
    m_h[6] += g;
    m_h[7] += h;
  }

  uint64_t m_len = 0;
  uint8_t m_buf[64];
  uint32_t m_h[8];
};

std::string sanitizeId(const std::string& name) {
  std::string id;
  for (char c : name) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      id += std::tolower(static_cast<unsigned char>(c));
    } else if (c == ' ' || c == '-' || c == '_') {
      if (!id.empty() && id.back() != '_') {
        id += '_';
      }
    }
  }
  while (!id.empty() && id.back() == '_') id.pop_back();
  return id.empty() ? "part" : id;
}

std::string getCurrentIsoTime() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

bool saveRgbaImageToPng(const std::string& filepath, int width, int height, const std::vector<uint32_t>& rgbaPixels) {
  FILE* fp = fopen(filepath.c_str(), "wb");
  if (!fp) return false;

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    fclose(fp);
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8,
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);

  std::vector<png_bytep> row_pointers(height);
  for (int y = 0; y < height; ++y) {
    row_pointers[y] = reinterpret_cast<png_bytep>(const_cast<uint32_t*>(&rgbaPixels[y * width]));
  }

  png_write_image(png_ptr, row_pointers.data());
  png_write_end(png_ptr, nullptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
  return true;
}

bool loadPngToRgba(const std::string& filepath, int& outWidth, int& outHeight, std::vector<uint32_t>& outPixels) {
  FILE* fp = fopen(filepath.c_str(), "rb");
  if (!fp) return false;

  png_byte header[8];
  if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
    fclose(fp);
    return false;
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    fclose(fp);
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  outWidth = png_get_image_width(png_ptr, info_ptr);
  outHeight = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  if (bit_depth == 16) png_set_strip_16(png_ptr);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  outPixels.resize(outWidth * outHeight);
  std::vector<png_bytep> row_pointers(outHeight);
  for (int y = 0; y < outHeight; ++y) {
    row_pointers[y] = reinterpret_cast<png_bytep>(&outPixels[y * outWidth]);
  }

  png_read_image(png_ptr, row_pointers.data());
  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
  fclose(fp);
  return true;
}

} // namespace

WcWorkspace::WcWorkspace() = default;
WcWorkspace::~WcWorkspace() = default;

std::string WcWorkspace::calculateFileSha256(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file) return "";
  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return calculateBufferSha256(buffer.data(), buffer.size());
}

std::string WcWorkspace::calculateBufferSha256(const uint8_t* data, size_t size) {
  Sha256 ctx;
  ctx.update(data, size);
  return ctx.finalize();
}

bool WcWorkspace::commitTempFile(const std::string& tempPath, const std::string& finalPath) {
  std::error_code ec;
  if (fs::exists(finalPath)) {
    fs::remove(finalPath, ec);
  }
  fs::rename(tempPath, finalPath, ec);
  if (ec) {
    fs::copy_file(tempPath, finalPath, fs::copy_options::overwrite_existing, ec);
    fs::remove(tempPath, ec);
  }
  return !ec;
}

bool WcWorkspace::open(const std::string& dirPath, std::string& errorMsg) {
  fs::path p(dirPath);
  if (!fs::exists(p) || !fs::is_directory(p)) {
    errorMsg = "Directory does not exist: " + dirPath;
    return false;
  }

  m_workspaceDir = dirPath;
  if (!loadWorkspaceJson(errorMsg)) {
    m_workspaceDir.clear();
    return false;
  }

  if (!loadCharacterJson(errorMsg)) {
    m_workspaceDir.clear();
    return false;
  }

  loadManifestJson(errorMsg);
  return true;
}

bool WcWorkspace::createNew(const std::string& dirPath, const std::string& characterName, int width, int height, std::string& errorMsg) {
  fs::path root(dirPath);
  std::error_code ec;
  fs::create_directories(root / "source" / "parts", ec);
  fs::create_directories(root / "animations", ec);
  fs::create_directories(root / "metadata", ec);

  m_workspaceDir = dirPath;

  std::string charId = sanitizeId(characterName);

  m_config.format = "wcworkspace";
  m_config.version = 2;
  m_config.workspaceId = charId;
  m_config.characterId = charId;
  m_config.characterName = characterName;
  m_config.canvas.width = width;
  m_config.canvas.height = height;
  m_config.pixelScale = 1;
  m_config.facing = "right";
  m_config.source.type = "aseprite";

  m_character.id = charId;
  m_character.name = characterName;
  m_character.parts.clear();

  m_manifest.version = 2;
  m_manifest.lastModified = getCurrentIsoTime();
  m_manifest.sourceParts.clear();
  m_manifest.animations.clear();

  if (!saveWorkspaceJson(errorMsg)) return false;
  if (!saveCharacterJson(errorMsg)) return false;
  if (!saveManifestJson(errorMsg)) return false;

  return true;
}

bool WcWorkspace::save(std::string& errorMsg) {
  if (!isOpen()) {
    errorMsg = "Workspace is not open";
    return false;
  }
  if (!saveWorkspaceJson(errorMsg)) return false;
  if (!saveCharacterJson(errorMsg)) return false;
  if (!saveManifestJson(errorMsg)) return false;
  return true;
}

bool WcWorkspace::loadWorkspaceJson(std::string& errorMsg) {
  fs::path filePath = fs::path(m_workspaceDir) / "workspace.json";
  if (!fs::exists(filePath)) {
    errorMsg = "workspace.json not found in " + m_workspaceDir;
    return false;
  }

  std::ifstream file(filePath);
  if (!file) {
    errorMsg = "Cannot read workspace.json";
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(content, err);
  if (!err.empty() || !json.is_object()) {
    errorMsg = "Invalid workspace.json: " + err;
    return false;
  }

  m_config.format = json["format"].string_value();
  m_config.version = json["version"].int_value();
  m_config.workspaceId = json["workspaceId"].string_value();

  if (json["character"].is_object()) {
    m_config.characterId = json["character"]["id"].string_value();
    m_config.characterName = json["character"]["name"].string_value();
  }

  if (json["canvas"].is_object()) {
    m_config.canvas.width = json["canvas"]["width"].int_value();
    m_config.canvas.height = json["canvas"]["height"].int_value();
  }

  m_config.pixelScale = json["pixelScale"].int_value() > 0 ? json["pixelScale"].int_value() : 1;
  m_config.facing = json["facing"].string_value().empty() ? "right" : json["facing"].string_value();

  if (json["source"].is_object()) {
    m_config.source.type = json["source"]["type"].string_value();
  }

  return true;
}

bool WcWorkspace::saveWorkspaceJson(std::string& errorMsg) {
  json11::Json root = json11::Json::object{
    {"format", m_config.format},
    {"version", m_config.version},
    {"workspaceId", m_config.workspaceId},
    {"character", json11::Json::object{
      {"id", m_config.characterId},
      {"name", m_config.characterName}
    }},
    {"canvas", json11::Json::object{
      {"width", m_config.canvas.width},
      {"height", m_config.canvas.height}
    }},
    {"pixelScale", m_config.pixelScale},
    {"facing", m_config.facing},
    {"source", json11::Json::object{
      {"type", m_config.source.type}
    }}
  };

  fs::path finalPath = fs::path(m_workspaceDir) / "workspace.json";
  fs::path tempPath = fs::path(m_workspaceDir) / "workspace.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot create temporary workspace.json";
    return false;
  }
  out << root.dump();
  out.close();

  return commitTempFile(tempPath.string(), finalPath.string());
}

bool WcWorkspace::loadCharacterJson(std::string& errorMsg) {
  fs::path filePath = fs::path(m_workspaceDir) / "source" / "character.json";
  if (!fs::exists(filePath)) {
    // If not found, fallback to check legacy or empty
    m_character.id = m_config.characterId;
    m_character.name = m_config.characterName;
    m_character.parts.clear();
    return true;
  }

  std::ifstream file(filePath);
  if (!file) {
    errorMsg = "Cannot read source/character.json";
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(content, err);
  if (!err.empty() || !json.is_object()) {
    errorMsg = "Invalid source/character.json: " + err;
    return false;
  }

  m_character.id = json["id"].string_value();
  m_character.name = json["name"].string_value();
  m_character.parts.clear();

  if (json["parts"].is_array()) {
    for (const auto& item : json["parts"].array_items()) {
      WcPart p;
      p.id = item["id"].string_value();
      p.name = item["name"].string_value();
      p.asset = item["asset"].string_value();
      if (item["pivot"].is_object()) {
        p.pivot.x = item["pivot"]["x"].number_value();
        p.pivot.y = item["pivot"]["y"].number_value();
      }
      p.zIndex = item["zIndex"].int_value();
      p.visible = item["visible"].bool_value();
      m_character.parts.push_back(p);
    }
  }

  return true;
}

bool WcWorkspace::saveCharacterJson(std::string& errorMsg) {
  json11::Json::array partsArr;
  for (const auto& p : m_character.parts) {
    partsArr.push_back(json11::Json::object{
      {"id", p.id},
      {"name", p.name},
      {"asset", p.asset},
      {"pivot", json11::Json::object{{"x", p.pivot.x}, {"y", p.pivot.y}}},
      {"zIndex", p.zIndex},
      {"visible", p.visible}
    });
  }

  json11::Json root = json11::Json::object{
    {"id", m_character.id},
    {"name", m_character.name},
    {"parts", partsArr}
  };

  fs::path dir = fs::path(m_workspaceDir) / "source";
  std::error_code ec;
  fs::create_directories(dir, ec);

  fs::path finalPath = dir / "character.json";
  fs::path tempPath = dir / "character.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot create temporary source/character.json";
    return false;
  }
  out << root.dump();
  out.close();

  return commitTempFile(tempPath.string(), finalPath.string());
}

bool WcWorkspace::loadManifestJson(std::string& errorMsg) {
  fs::path filePath = fs::path(m_workspaceDir) / "metadata" / "manifest.json";
  if (!fs::exists(filePath)) {
    m_manifest.version = 2;
    m_manifest.lastModified = getCurrentIsoTime();
    m_manifest.sourceParts.clear();
    m_manifest.animations.clear();
    return true;
  }

  std::ifstream file(filePath);
  if (!file) return true;

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(content, err);
  if (!err.empty() || !json.is_object()) return true;

  m_manifest.version = json["version"].int_value();
  m_manifest.lastModified = json["lastModified"].string_value();
  m_manifest.sourceParts.clear();
  m_manifest.animations.clear();

  if (json["sources"].is_object() && json["sources"]["parts"].is_object()) {
    for (const auto& kv : json["sources"]["parts"].object_items()) {
      m_manifest.sourceParts[kv.first] = kv.second.string_value();
    }
  }

  if (json["animations"].is_object()) {
    for (const auto& kv : json["animations"].object_items()) {
      WcAnimManifestItem item;
      item.status = kv.second["status"].string_value();
      item.generatedHash = kv.second["generatedHash"].string_value();
      item.polishedHash = kv.second["polishedHash"].string_value();
      item.spriteSheetHash = kv.second["spriteSheetHash"].string_value();
      item.frameCount = kv.second["frameCount"].int_value();
      m_manifest.animations[kv.first] = item;
    }
  }

  return true;
}

bool WcWorkspace::saveManifestJson(std::string& errorMsg) {
  json11::Json::object partsObj;
  for (const auto& kv : m_manifest.sourceParts) {
    partsObj[kv.first] = kv.second;
  }

  json11::Json::object animsObj;
  for (const auto& kv : m_manifest.animations) {
    animsObj[kv.first] = json11::Json::object{
      {"status", kv.second.status},
      {"generatedHash", kv.second.generatedHash},
      {"polishedHash", kv.second.polishedHash},
      {"spriteSheetHash", kv.second.spriteSheetHash},
      {"frameCount", kv.second.frameCount}
    };
  }

  json11::Json root = json11::Json::object{
    {"version", m_manifest.version},
    {"lastModified", getCurrentIsoTime()},
    {"sources", json11::Json::object{
      {"parts", partsObj}
    }},
    {"animations", animsObj}
  };

  fs::path dir = fs::path(m_workspaceDir) / "metadata";
  std::error_code ec;
  fs::create_directories(dir, ec);

  fs::path finalPath = dir / "manifest.json";
  fs::path tempPath = dir / "manifest.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot write temporary manifest.json";
    return false;
  }
  out << root.dump();
  out.close();

  return commitTempFile(tempPath.string(), finalPath.string());
}

bool WcWorkspace::loadAnimJson(const std::string& animId, WcAnimationConfig& outConfig, std::string& errorMsg) const {
  fs::path animJsonPath = fs::path(m_workspaceDir) / "animations" / animId / "animation.json";
  if (!fs::exists(animJsonPath)) {
    errorMsg = "animation.json not found for " + animId;
    return false;
  }

  std::ifstream file(animJsonPath);
  if (!file) {
    errorMsg = "Cannot read animation.json for " + animId;
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(content, err);
  if (!err.empty() || !json.is_object()) {
    errorMsg = "Invalid animation.json: " + err;
    return false;
  }

  outConfig.id = json["id"].string_value();
  outConfig.name = json["name"].string_value();
  outConfig.fps = json["fps"].int_value() > 0 ? json["fps"].int_value() : 12;
  outConfig.frameCount = json["frameCount"].int_value();
  if (json["canvas"].is_object()) {
    outConfig.canvas.width = json["canvas"]["width"].int_value();
    outConfig.canvas.height = json["canvas"]["height"].int_value();
  }
  outConfig.source = json["source"].string_value();
  outConfig.status = json["status"].string_value();

  return true;
}

bool WcWorkspace::saveAnimJson(const std::string& animId, const WcAnimationConfig& animConfig, std::string& errorMsg) {
  json11::Json root = json11::Json::object{
    {"id", animConfig.id},
    {"name", animConfig.name},
    {"fps", animConfig.fps},
    {"frameCount", animConfig.frameCount},
    {"canvas", json11::Json::object{
      {"width", animConfig.canvas.width},
      {"height", animConfig.canvas.height}
    }},
    {"source", animConfig.source},
    {"status", animConfig.status}
  };

  fs::path dir = fs::path(m_workspaceDir) / "animations" / animId;
  std::error_code ec;
  fs::create_directories(dir, ec);

  fs::path finalPath = dir / "animation.json";
  fs::path tempPath = dir / "animation.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot write animation.json for " + animId;
    return false;
  }
  out << root.dump();
  out.close();

  return commitTempFile(tempPath.string(), finalPath.string());
}

bool WcWorkspace::exportCharacterParts(app::Doc* doc, std::string& outSummary, std::string& errorMsg) {
  if (!doc || !doc->sprite()) {
    errorMsg = "No active document or sprite";
    return false;
  }

  doc::Sprite* spr = doc->sprite();
  int w = spr->width();
  int h = spr->height();

  fs::path partsDir = fs::path(m_workspaceDir) / "source" / "parts";
  std::error_code ec;
  fs::create_directories(partsDir, ec);

  render::Render renderer;
  std::vector<std::string> updatedParts;
  std::vector<WcPart> newParts;
  int zIndex = 0;

  for (doc::Layer* layer : spr->allLayers()) {
    if (!layer->isImage() || layer->isGroup()) continue;

    std::string layerName = layer->name();
    std::string partId = sanitizeId(layerName);
    std::string filename = partId + ".png";
    fs::path finalPng = partsDir / filename;
    fs::path tempPng = partsDir / (filename + ".tmp");

    std::unique_ptr<doc::Image> renderedImg(doc::Image::create(doc::IMAGE_RGB, w, h));
    renderedImg->clear(0);
    renderer.renderLayer(renderedImg.get(), layer, 0);

    std::vector<uint32_t> rgbaPixels(w * h);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        doc::color_t c = renderedImg->getPixel(x, y);
        uint8_t r = doc::rgba_getr(c);
        uint8_t g = doc::rgba_getg(c);
        uint8_t b = doc::rgba_getb(c);
        uint8_t a = doc::rgba_geta(c);
        rgbaPixels[y * w + x] = (static_cast<uint32_t>(a) << 24) |
                                (static_cast<uint32_t>(b) << 16) |
                                (static_cast<uint32_t>(g) << 8) |
                                (static_cast<uint32_t>(r));
      }
    }

    std::string sha = calculateBufferSha256(reinterpret_cast<const uint8_t*>(rgbaPixels.data()), rgbaPixels.size() * 4);

    bool changed = true;
    auto it = m_manifest.sourceParts.find(partId);
    if (it != m_manifest.sourceParts.end() && it->second == sha && fs::exists(finalPng)) {
      changed = false;
    }

    if (changed) {
      if (!saveRgbaImageToPng(tempPng.string(), w, h, rgbaPixels)) {
        errorMsg = "Failed to write PNG for part: " + layerName;
        return false;
      }
      if (!commitTempFile(tempPng.string(), finalPng.string())) {
        errorMsg = "Failed to commit part image: " + layerName;
        return false;
      }
      m_manifest.sourceParts[partId] = sha;
      updatedParts.push_back(layerName);
    }

    WcPart part;
    part.id = partId;
    part.name = layerName;
    part.asset = "source/parts/" + filename; // Portable relative path
    part.pivot = { static_cast<double>(w) / 2.0, static_cast<double>(h) / 2.0 };
    part.zIndex = zIndex++;
    part.visible = layer->isVisible();
    newParts.push_back(part);
  }

  m_config.canvas.width = w;
  m_config.canvas.height = h;
  m_character.parts = newParts;

  if (!saveWorkspaceJson(errorMsg)) return false;
  if (!saveCharacterJson(errorMsg)) return false;
  if (!saveManifestJson(errorMsg)) return false;

  std::stringstream ss;
  ss << "Wild Conquest Character Parts Exported:\n";
  ss << "Character: " << m_character.name << " (" << m_character.parts.size() << " parts)\n";
  if (updatedParts.empty()) {
    ss << "All parts are already up to date.\n";
  } else {
    ss << "Updated " << updatedParts.size() << " part(s):\n";
    for (const auto& name : updatedParts) {
      ss << "  ✓ " << name << "\n";
    }
  }
  outSummary = ss.str();
  return true;
}

bool WcWorkspace::getAvailableAnimations(std::vector<std::string>& outAnimIds, std::string& errorMsg) const {
  outAnimIds.clear();
  fs::path animsDir = fs::path(m_workspaceDir) / "animations";
  if (!fs::exists(animsDir) || !fs::is_directory(animsDir)) {
    return true;
  }

  for (const auto& entry : fs::directory_iterator(animsDir)) {
    if (entry.is_directory()) {
      outAnimIds.push_back(entry.path().filename().string());
    }
  }
  std::sort(outAnimIds.begin(), outAnimIds.end());
  return true;
}

bool WcWorkspace::syncAnimationToAseprite(const std::string& animId, app::Context* ctx, std::string& outSummary, std::string& errorMsg) {
  fs::path animDir = fs::path(m_workspaceDir) / "animations" / animId;
  fs::path genDir = animDir / "generated";
  fs::path polDir = animDir / "polished";

  fs::path sourceFramesDir;
  if (fs::exists(polDir) && !fs::is_empty(polDir)) {
    sourceFramesDir = polDir;
  } else if (fs::exists(genDir) && !fs::is_empty(genDir)) {
    sourceFramesDir = genDir;
  } else {
    errorMsg = "No frames found in generated/ or polished/ for animation: " + animId;
    return false;
  }

  std::vector<fs::path> frameFiles;
  for (const auto& entry : fs::directory_iterator(sourceFramesDir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".png") {
      frameFiles.push_back(entry.path());
    }
  }
  std::sort(frameFiles.begin(), frameFiles.end());

  if (frameFiles.empty()) {
    errorMsg = "No PNG frames found in " + sourceFramesDir.string();
    return false;
  }

  WcAnimationConfig animCfg;
  if (!loadAnimJson(animId, animCfg, errorMsg)) {
    animCfg.id = animId;
    animCfg.name = animId;
    animCfg.fps = 12;
    animCfg.frameCount = static_cast<int>(frameFiles.size());
    animCfg.canvas = m_config.canvas;
  }

  int targetW = animCfg.canvas.width;
  int targetH = animCfg.canvas.height;
  int frameDurationMs = animCfg.fps > 0 ? (1000 / animCfg.fps) : 83;

  // Create new sprite document in Aseprite
  std::unique_ptr<doc::Sprite> spr(new doc::Sprite(doc::ImageSpec(doc::ColorMode::RGB, targetW, targetH), 256));
  spr->setTotalFrames(doc::frame_t(frameFiles.size()));

  doc::LayerImage* layer = new doc::LayerImage(spr.get());
  layer->setName(animId + "_frames");
  spr->root()->addLayer(layer);

  for (size_t i = 0; i < frameFiles.size(); ++i) {
    spr->setFrameDuration(doc::frame_t(i), frameDurationMs);
    int imgW = 0, imgH = 0;
    std::vector<uint32_t> pixels;
    if (loadPngToRgba(frameFiles[i].string(), imgW, imgH, pixels)) {
      std::unique_ptr<doc::Image> img(doc::Image::create(doc::IMAGE_RGB, imgW, imgH));
      for (int y = 0; y < imgH; ++y) {
        for (int x = 0; x < imgW; ++x) {
          uint32_t c = pixels[y * imgW + x];
          uint8_t r = c & 0xFF;
          uint8_t g = (c >> 8) & 0xFF;
          uint8_t b = (c >> 16) & 0xFF;
          uint8_t a = (c >> 24) & 0xFF;
          img->putPixel(x, y, doc::rgba(r, g, b, a));
        }
      }
      doc::Cel* cel = new doc::Cel(doc::frame_t(i), doc::ImageRef(img.release()));
      layer->addCel(cel);
    }
  }

  std::unique_ptr<app::Doc> newDoc(new app::Doc(spr.release()));
  newDoc->setFilename((animDir / (animId + ".aseprite")).string());
  ctx->documents().add(newDoc.release());

  // Update animation status to polishing
  animCfg.status = "polishing";
  saveAnimJson(animId, animCfg, errorMsg);

  m_manifest.animations[animId].status = "polishing";
  m_manifest.animations[animId].frameCount = static_cast<int>(frameFiles.size());
  saveManifestJson(errorMsg);

  outSummary = "Loaded " + std::to_string(frameFiles.size()) + " frames into Aseprite for animation: " + animId + " (" + std::to_string(animCfg.fps) + " FPS)";
  return true;
}

bool WcWorkspace::exportPolishedAnimation(app::Doc* doc, const std::string& animId, bool overwritePolished, std::string& outSummary, std::string& errorMsg) {
  if (!doc || !doc->sprite()) {
    errorMsg = "No active document or sprite";
    return false;
  }

  doc::Sprite* spr = doc->sprite();
  int frameCount = spr->totalFrames();
  int w = spr->width();
  int h = spr->height();

  fs::path animDir = fs::path(m_workspaceDir) / "animations" / animId;
  fs::path polishedDir = animDir / "polished";
  fs::path sheetDir = animDir / "sprite-sheet";

  // Overwrite protection
  if (fs::exists(polishedDir) && !fs::is_empty(polishedDir) && !overwritePolished) {
    errorMsg = "Polished frames already exist in: " + polishedDir.string() + "\nExport cancelled to prevent overwriting.";
    return false;
  }

  std::error_code ec;
  fs::create_directories(polishedDir, ec);
  fs::create_directories(sheetDir, ec);

  render::Render renderer;
  std::vector<uint32_t> sheetPixels(w * frameCount * h, 0);
  Sha256 framesCombinedHash;

  for (doc::frame_t frame = 0; frame < frameCount; ++frame) {
    std::unique_ptr<doc::Image> renderedImg(doc::Image::create(doc::IMAGE_RGB, w, h));
    renderedImg->clear(0);
    renderer.renderSprite(renderedImg.get(), spr, frame);

    std::vector<uint32_t> rgba(w * h);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        doc::color_t c = renderedImg->getPixel(x, y);
        uint8_t r = doc::rgba_getr(c);
        uint8_t g = doc::rgba_getg(c);
        uint8_t b = doc::rgba_getb(c);
        uint8_t a = doc::rgba_geta(c);
        uint32_t packed = (static_cast<uint32_t>(a) << 24) |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          (static_cast<uint32_t>(r));
        rgba[y * w + x] = packed;

        // Copy into horizontal sprite sheet
        int sheetX = frame * w + x;
        sheetPixels[y * (w * frameCount) + sheetX] = packed;
      }
    }

    framesCombinedHash.update(reinterpret_cast<const uint8_t*>(rgba.data()), rgba.size() * 4);

    std::ostringstream fn;
    fn << std::setw(3) << std::setfill('0') << frame << ".png";
    fs::path framePath = polishedDir / fn.str();
    if (!saveRgbaImageToPng(framePath.string(), w, h, rgba)) {
      errorMsg = "Failed to save polished frame " + fn.str();
      return false;
    }
  }

  std::string polishedHash = framesCombinedHash.finalize();

  // Save sprite sheet
  fs::path sheetFile = sheetDir / (animId + ".png");
  if (!saveRgbaImageToPng(sheetFile.string(), w * frameCount, h, sheetPixels)) {
    errorMsg = "Failed to save sprite sheet " + sheetFile.string();
    return false;
  }
  std::string sheetHash = calculateBufferSha256(reinterpret_cast<const uint8_t*>(sheetPixels.data()), sheetPixels.size() * 4);

  // Update animation.json
  WcAnimationConfig animCfg;
  if (!loadAnimJson(animId, animCfg, errorMsg)) {
    animCfg.id = animId;
    animCfg.name = animId;
    animCfg.canvas.width = w;
    animCfg.canvas.height = h;
  }
  animCfg.frameCount = frameCount;
  animCfg.status = "polished";
  saveAnimJson(animId, animCfg, errorMsg);

  // Update manifest.json
  m_manifest.animations[animId].status = "polished";
  m_manifest.animations[animId].polishedHash = polishedHash;
  m_manifest.animations[animId].spriteSheetHash = sheetHash;
  m_manifest.animations[animId].frameCount = frameCount;
  saveManifestJson(errorMsg);

  std::stringstream ss;
  ss << "Exported Polished Animation (" << animId << "):\n";
  ss << "  ✓ Frames: " << frameCount << " -> " << polishedDir.string() << "\n";
  ss << "  ✓ Sprite Sheet: " << (w * frameCount) << "x" << h << " -> " << sheetFile.string() << "\n";
  outSummary = ss.str();
  return true;
}

bool WcWorkspace::validate(std::vector<std::string>& outErrors, std::vector<std::string>& outWarnings) const {
  outErrors.clear();
  outWarnings.clear();

  if (!isOpen()) {
    outErrors.push_back("Workspace is not opened.");
    return false;
  }

  fs::path root(m_workspaceDir);
  if (!fs::exists(root / "workspace.json")) {
    outErrors.push_back("workspace.json missing.");
  }
  if (!fs::exists(root / "source" / "character.json")) {
    outErrors.push_back("source/character.json missing.");
  }

  fs::path partsDir = root / "source" / "parts";
  if (!fs::exists(partsDir)) {
    outErrors.push_back("source/parts directory missing.");
  } else {
    for (const auto& part : m_character.parts) {
      fs::path partPath = root / part.asset;
      if (!fs::exists(partPath)) {
        outErrors.push_back("Part asset missing: " + part.asset);
      }
    }
  }

  fs::path animsDir = root / "animations";
  if (fs::exists(animsDir) && fs::is_directory(animsDir)) {
    for (const auto& entry : fs::directory_iterator(animsDir)) {
      if (!entry.is_directory()) continue;
      std::string animId = entry.path().filename().string();
      fs::path animJson = entry.path() / "animation.json";
      if (!fs::exists(animJson)) {
        outWarnings.push_back("Animation missing animation.json: " + animId);
      }
    }
  }

  return outErrors.empty();
}

static WcWorkspace g_activeWorkspace;

WcWorkspace& getActiveWorkspace() {
  return g_activeWorkspace;
}

} // namespace wildconquest
