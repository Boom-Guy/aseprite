// Wild Conquest - Shared Workspace Implementation
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

#include "app/doc.h"
#include "app/context.h"
#include "app/cmd/add_slice.h"
#include "app/cmd/add_tag.h"
#include "app/file/file.h"
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

// Helper: Normalize name to ID (e.g. "Left Arm" -> "left_arm")
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

// Simple 64-bit FNV-1a Hash converted to hex string
std::string fnv1a64Hex(const uint8_t* data, size_t size) {
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < size; ++i) {
    hash ^= data[i];
    hash *= 1099511628211ULL;
  }
  std::stringstream ss;
  ss << std::hex << std::setw(16) << std::setfill('0') << hash;
  return ss.str();
}

std::string getCurrentIsoTime() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

// Low-level helper to write RGBA Image to PNG using libpng
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

// Low-level helper to read PNG into RGBA pixel vector
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

std::string WcWorkspace::calculateFileHash(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file) return "";
  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return calculateBufferHash(buffer.data(), buffer.size());
}

std::string WcWorkspace::calculateBufferHash(const uint8_t* data, size_t size) {
  return fnv1a64Hex(data, size);
}

bool WcWorkspace::commitTempFile(const std::string& tempPath, const std::string& finalPath) {
  std::error_code ec;
  if (fs::exists(finalPath)) {
    fs::remove(finalPath, ec);
  }
  fs::rename(tempPath, finalPath, ec);
  if (ec) {
    // Try copy and remove if cross-device rename fails
    fs::copy_file(tempPath, finalPath, fs::copy_options::overwrite_existing, ec);
    fs::remove(tempPath, ec);
  }
  return !ec;
}

bool WcWorkspace::open(const std::string& dirPath, std::string& errorMsg) {
  fs::path p(dirPath);
  if (!fs::exists(p) || !fs::is_directory(p)) {
    errorMsg = "Path does not exist or is not a directory: " + dirPath;
    return false;
  }

  m_workspaceDir = dirPath;
  if (!loadManifest(errorMsg)) {
    m_workspaceDir.clear();
    return false;
  }

  loadSyncManifest(errorMsg);
  return true;
}

bool WcWorkspace::createNew(const std::string& dirPath, const std::string& characterName, int width, int height, std::string& errorMsg) {
  fs::path p(dirPath);
  std::error_code ec;
  fs::create_directories(p / "parts", ec);
  fs::create_directories(p / "animations", ec);
  fs::create_directories(p / "frames", ec);

  m_workspaceDir = dirPath;
  m_manifest.format = "wcworkspace";
  m_manifest.version = 1;
  m_manifest.name = characterName;
  m_manifest.character = sanitizeId(characterName);
  m_manifest.workspaceId = "ws_" + m_manifest.character;
  m_manifest.canvas.width = width;
  m_manifest.canvas.height = height;
  m_manifest.pixelScale = 1;
  m_manifest.facing = "right";
  m_manifest.groundY = height > 5 ? height - 5 : height - 1;
  m_manifest.parts.clear();

  m_sync.version = 1;
  m_sync.lastSyncTime = getCurrentIsoTime();
  m_sync.parts.clear();
  m_sync.animations.clear();

  if (!saveManifest(errorMsg)) return false;
  if (!saveSyncManifest(errorMsg)) return false;

  return true;
}

bool WcWorkspace::loadManifest(std::string& errorMsg) {
  fs::path manifestPath = fs::path(m_workspaceDir) / "workspace.json";
  if (!fs::exists(manifestPath)) {
    errorMsg = "workspace.json not found in " + m_workspaceDir;
    return false;
  }

  std::ifstream file(manifestPath);
  if (!file) {
    errorMsg = "Failed to open workspace.json";
    return false;
  }

  std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(str, err);
  if (!err.empty() || !json.is_object()) {
    errorMsg = "JSON parse error in workspace.json: " + err;
    return false;
  }

  m_manifest.format = json["format"].string_value();
  m_manifest.version = json["version"].int_value();
  m_manifest.workspaceId = json["workspaceId"].string_value();
  m_manifest.name = json["name"].string_value();
  m_manifest.character = json["character"].string_value();
  if (json["canvas"].is_object()) {
    m_manifest.canvas.width = json["canvas"]["width"].int_value();
    m_manifest.canvas.height = json["canvas"]["height"].int_value();
  }
  m_manifest.pixelScale = json["pixelScale"].int_value() > 0 ? json["pixelScale"].int_value() : 1;
  m_manifest.facing = json["facing"].string_value().empty() ? "right" : json["facing"].string_value();
  m_manifest.groundY = json["groundY"].int_value();

  m_manifest.parts.clear();
  if (json["parts"].is_array()) {
    for (const auto& item : json["parts"].array_items()) {
      WcPart part;
      part.id = item["id"].string_value();
      part.name = item["name"].string_value();
      part.asset = item["asset"].string_value();
      if (item["pivot"].is_object()) {
        part.pivot.x = item["pivot"]["x"].number_value();
        part.pivot.y = item["pivot"]["y"].number_value();
      }
      part.zIndex = item["zIndex"].int_value();
      part.visible = item["visible"].bool_value();
      m_manifest.parts.push_back(part);
    }
  }

  return true;
}

bool WcWorkspace::saveManifest(std::string& errorMsg) {
  json11::Json::array partsArray;
  for (const auto& p : m_manifest.parts) {
    json11::Json::object partObj;
    partObj["id"] = p.id;
    partObj["name"] = p.name;
    partObj["asset"] = p.asset;
    partObj["pivot"] = json11::Json::object{{"x", p.pivot.x}, {"y", p.pivot.y}};
    partObj["zIndex"] = p.zIndex;
    partObj["visible"] = p.visible;
    partsArray.push_back(partObj);
  }

  json11::Json root = json11::Json::object{
    {"format", m_manifest.format},
    {"version", m_manifest.version},
    {"workspaceId", m_manifest.workspaceId},
    {"name", m_manifest.name},
    {"character", m_manifest.character},
    {"canvas", json11::Json::object{
      {"width", m_manifest.canvas.width},
      {"height", m_manifest.canvas.height}
    }},
    {"pixelScale", m_manifest.pixelScale},
    {"facing", m_manifest.facing},
    {"groundY", m_manifest.groundY},
    {"parts", partsArray}
  };

  fs::path manifestPath = fs::path(m_workspaceDir) / "workspace.json";
  fs::path tempPath = fs::path(m_workspaceDir) / "workspace.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot create temporary workspace manifest";
    return false;
  }
  out << root.dump();
  out.close();

  if (!commitTempFile(tempPath.string(), manifestPath.string())) {
    errorMsg = "Failed to atomically write workspace.json";
    return false;
  }
  return true;
}

bool WcWorkspace::loadSyncManifest(std::string& errorMsg) {
  fs::path syncPath = fs::path(m_workspaceDir) / "sync.json";
  if (!fs::exists(syncPath)) {
    m_sync.version = 1;
    m_sync.lastSyncTime = getCurrentIsoTime();
    return true;
  }

  std::ifstream file(syncPath);
  if (!file) return true;

  std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  auto json = json11::Json::parse(str, err);
  if (!err.empty() || !json.is_object()) return true;

  m_sync.version = json["version"].int_value();
  m_sync.lastSyncTime = json["lastSyncTime"].string_value();
  m_sync.parts.clear();
  if (json["parts"].is_object()) {
    for (const auto& kv : json["parts"].object_items()) {
      WcPartSyncInfo info;
      info.version = kv.second["version"].int_value();
      info.hash = kv.second["hash"].string_value();
      info.source = kv.second["source"].string_value();
      m_sync.parts[kv.first] = info;
    }
  }

  m_sync.animations.clear();
  if (json["animations"].is_object()) {
    for (const auto& kv : json["animations"].object_items()) {
      WcAnimSyncInfo info;
      info.version = kv.second["version"].int_value();
      info.source = kv.second["source"].string_value();
      info.frameCount = kv.second["frameCount"].int_value();
      info.hash = kv.second["hash"].string_value();
      m_sync.animations[kv.first] = info;
    }
  }

  return true;
}

bool WcWorkspace::saveSyncManifest(std::string& errorMsg) {
  json11::Json::object partsObj;
  for (const auto& kv : m_sync.parts) {
    partsObj[kv.first] = json11::Json::object{
      {"version", kv.second.version},
      {"hash", kv.second.hash},
      {"source", kv.second.source}
    };
  }

  json11::Json::object animsObj;
  for (const auto& kv : m_sync.animations) {
    animsObj[kv.first] = json11::Json::object{
      {"version", kv.second.version},
      {"source", kv.second.source},
      {"frameCount", kv.second.frameCount},
      {"hash", kv.second.hash}
    };
  }

  json11::Json root = json11::Json::object{
    {"version", m_sync.version},
    {"lastSyncTime", m_sync.lastSyncTime},
    {"parts", partsObj},
    {"animations", animsObj}
  };

  fs::path syncPath = fs::path(m_workspaceDir) / "sync.json";
  fs::path tempPath = fs::path(m_workspaceDir) / "sync.json.tmp";

  std::ofstream out(tempPath);
  if (!out) {
    errorMsg = "Cannot create temporary sync manifest";
    return false;
  }
  out << root.dump();
  out.close();

  return commitTempFile(tempPath.string(), syncPath.string());
}

bool WcWorkspace::exportLayersToWorkspace(app::Doc* doc, std::string& outSummary, std::string& errorMsg) {
  if (!doc || !doc->sprite()) {
    errorMsg = "No active document or sprite";
    return false;
  }

  doc::Sprite* spr = doc->sprite();
  int w = spr->width();
  int h = spr->height();

  std::vector<std::string> updatedParts;
  render::Render renderer;

  fs::path partsDir = fs::path(m_workspaceDir) / "parts";
  std::error_code ec;
  fs::create_directories(partsDir, ec);

  int zIndex = 0;
  std::vector<WcPart> newParts;

  for (doc::Layer* layer : spr->allLayers()) {
    if (!layer->isImage() || layer->isGroup()) continue;

    std::string layerName = layer->name();
    std::string partId = sanitizeId(layerName);
    std::string filename = partId + ".png";
    fs::path finalPng = partsDir / filename;
    fs::path tempPng = partsDir / (filename + ".tmp");

    // Render this layer at frame 0 into an RGBA pixel buffer
    std::unique_ptr<doc::Image> renderedImg(doc::Image::create(doc::IMAGE_RGB, w, h));
    renderedImg->clear(0);
    renderer.renderLayer(renderedImg.get(), layer, 0);

    // Convert doc::Image to RGBA vector
    std::vector<uint32_t> rgbaPixels(w * h);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        doc::color_t c = renderedImg->getPixel(x, y);
        uint8_t r = doc::rgba_getr(c);
        uint8_t g = doc::rgba_getg(c);
        uint8_t b = doc::rgba_getb(c);
        uint8_t a = doc::rgba_geta(c);
        // RGBA in memory
        rgbaPixels[y * w + x] = (static_cast<uint32_t>(a) << 24) |
                                (static_cast<uint32_t>(b) << 16) |
                                (static_cast<uint32_t>(g) << 8) |
                                (static_cast<uint32_t>(r));
      }
    }

    std::string newHash = calculateBufferHash(reinterpret_cast<const uint8_t*>(rgbaPixels.data()), rgbaPixels.size() * 4);

    bool hasChanged = true;
    auto it = m_sync.parts.find(partId);
    if (it != m_sync.parts.end() && it->second.hash == newHash && fs::exists(finalPng)) {
      hasChanged = false;
    }

    if (hasChanged) {
      if (!saveRgbaImageToPng(tempPng.string(), w, h, rgbaPixels)) {
        errorMsg = "Failed to write temporary image for part " + layerName;
        return false;
      }
      if (!commitTempFile(tempPng.string(), finalPng.string())) {
        errorMsg = "Failed to commit part " + layerName;
        return false;
      }
      m_sync.parts[partId].version = (it != m_sync.parts.end()) ? it->second.version + 1 : 1;
      m_sync.parts[partId].hash = newHash;
      m_sync.parts[partId].source = "aseprite";
      updatedParts.push_back(layerName);
    }

    WcPart part;
    part.id = partId;
    part.name = layerName;
    part.asset = "parts/" + filename;
    part.pivot = { static_cast<double>(w) / 2.0, static_cast<double>(h) / 2.0 };
    part.zIndex = zIndex++;
    part.visible = layer->isVisible();
    newParts.push_back(part);
  }

  m_manifest.canvas.width = w;
  m_manifest.canvas.height = h;
  m_manifest.parts = newParts;
  m_sync.lastSyncTime = getCurrentIsoTime();

  if (!saveManifest(errorMsg)) return false;
  if (!saveSyncManifest(errorMsg)) return false;

  std::stringstream ss;
  ss << "Wild Conquest Workspace Updated:\n";
  if (updatedParts.empty()) {
    ss << "Everything is already synchronized (0 parts modified).\n";
  } else {
    ss << "Updated " << updatedParts.size() << " part(s):\n";
    for (const auto& name : updatedParts) {
      ss << "  ✓ " << name << "\n";
    }
  }
  outSummary = ss.str();
  return true;
}

bool WcWorkspace::syncToSkelForm(app::Doc* doc, std::string& outSummary, std::string& errorMsg) {
  return exportLayersToWorkspace(doc, outSummary, errorMsg);
}

bool WcWorkspace::refreshFromWorkspace(app::Doc* doc, app::Context* ctx, std::string& outSummary, std::string& errorMsg) {
  if (!doc || !doc->sprite()) {
    errorMsg = "No active document";
    return false;
  }

  fs::path framesDir = fs::path(m_workspaceDir) / "frames";
  if (!fs::exists(framesDir) || !fs::is_directory(framesDir)) {
    outSummary = "No rendered frames found in workspace.\n";
    return true;
  }

  doc::Sprite* spr = doc->sprite();
  std::vector<std::string> updatedAnims;

  // Iterate over animation directories in frames/
  for (const auto& entry : fs::directory_iterator(framesDir)) {
    if (!entry.is_directory()) continue;

    std::string animName = entry.path().filename().string();
    std::vector<fs::path> frameFiles;
    for (const auto& frameEntry : fs::directory_iterator(entry.path())) {
      if (frameEntry.path().extension() == ".png") {
        frameFiles.push_back(frameEntry.path());
      }
    }
    std::sort(frameFiles.begin(), frameFiles.end());
    if (frameFiles.empty()) continue;

    updatedAnims.push_back(animName + " (" + std::to_string(frameFiles.size()) + " frames)");
  }

  std::stringstream ss;
  ss << "Wild Conquest Refresh:\n";
  if (updatedAnims.empty()) {
    ss << "No rendered animation frames available from SkelForm yet.\n";
  } else {
    ss << "Detected SkelForm Animation Frames:\n";
    for (const auto& anim : updatedAnims) {
      ss << "  ✓ " << anim << "\n";
    }
  }
  outSummary = ss.str();
  return true;
}

static WcWorkspace g_activeWorkspace;

WcWorkspace& getActiveWorkspace() {
  return g_activeWorkspace;
}

} // namespace wildconquest
