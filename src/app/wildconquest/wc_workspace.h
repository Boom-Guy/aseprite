// Wild Conquest - Shared Workspace Module (v2 Round-Trip Pipeline)
// Copyright (C) 2026 Wild Conquest Team

#ifndef APP_WILDCONQUEST_WC_WORKSPACE_H_INCLUDED
#define APP_WILDCONQUEST_WC_WORKSPACE_H_INCLUDED
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace app {
class Doc;
class Context;
} // namespace app

namespace doc {
class Sprite;
class Layer;
class Image;
} // namespace doc

namespace wildconquest {

struct WcCanvas {
  int width = 64;
  int height = 64;
};

struct WcVec2 {
  double x = 0.0;
  double y = 0.0;
};

struct WcPart {
  std::string id;
  std::string name;
  std::string asset; // Relative path, e.g. "source/parts/head.png"
  WcVec2 pivot{32.0, 32.0};
  int zIndex = 0;
  bool visible = true;
};

struct WcCharacterConfig {
  std::string id;
  std::string name;
  std::vector<WcPart> parts;
};

struct WcSourceInfo {
  std::string type = "aseprite";
};

struct WcWorkspaceConfig {
  std::string format = "wcworkspace";
  int version = 2;
  std::string workspaceId;
  std::string characterId;
  std::string characterName;
  WcCanvas canvas{64, 64};
  int pixelScale = 1;
  std::string facing = "right";
  WcSourceInfo source{"aseprite"};
};

struct WcAnimationConfig {
  std::string id;
  std::string name;
  int fps = 12;
  int frameCount = 0;
  WcCanvas canvas{64, 64};
  std::string source = "unity";
  std::string status = "not_generated"; // not_generated, generated, polishing, polished, imported_to_unity
};

struct WcAnimManifestItem {
  std::string status = "not_generated";
  std::string generatedHash;
  std::string polishedHash;
  std::string spriteSheetHash;
  int frameCount = 0;
};

struct WcManifest {
  int version = 2;
  std::string lastModified;
  std::map<std::string, std::string> sourceParts; // partId -> sha256
  std::map<std::string, WcAnimManifestItem> animations; // animId -> manifest item
};

class WcWorkspace {
public:
  WcWorkspace();
  ~WcWorkspace();

  bool isOpen() const { return !m_workspaceDir.empty(); }
  const std::string& workspaceDir() const { return m_workspaceDir; }
  const WcWorkspaceConfig& config() const { return m_config; }
  const WcCharacterConfig& character() const { return m_character; }
  const WcManifest& manifest() const { return m_manifest; }

  // File I/O & Creation
  bool open(const std::string& dirPath, std::string& errorMsg);
  bool createNew(const std::string& dirPath, const std::string& characterName, int width, int height, std::string& errorMsg);
  bool save(std::string& errorMsg);

  // Aseprite Export -> Workspace
  bool exportCharacterParts(app::Doc* doc, std::string& outSummary, std::string& errorMsg);

  // Sync Animation from Unity/Workspace into Aseprite
  bool getAvailableAnimations(std::vector<std::string>& outAnimIds, std::string& errorMsg) const;
  bool syncAnimationToAseprite(const std::string& animId, app::Context* ctx, std::string& outSummary, std::string& errorMsg);

  // Aseprite Polish -> Export Polished Frames & Sprite Sheet
  bool exportPolishedAnimation(app::Doc* doc, const std::string& animId, bool overwritePolished, std::string& outSummary, std::string& errorMsg);

  // Validation
  bool validate(std::vector<std::string>& outErrors, std::vector<std::string>& outWarnings) const;

  // SHA-256 Hashing helpers
  static std::string calculateFileSha256(const std::string& filePath);
  static std::string calculateBufferSha256(const uint8_t* data, size_t size);

private:
  std::string m_workspaceDir;
  WcWorkspaceConfig m_config;
  WcCharacterConfig m_character;
  WcManifest m_manifest;

  bool loadWorkspaceJson(std::string& errorMsg);
  bool saveWorkspaceJson(std::string& errorMsg);

  bool loadCharacterJson(std::string& errorMsg);
  bool saveCharacterJson(std::string& errorMsg);

  bool loadManifestJson(std::string& errorMsg);
  bool saveManifestJson(std::string& errorMsg);

  bool loadAnimJson(const std::string& animId, WcAnimationConfig& outConfig, std::string& errorMsg) const;
  bool saveAnimJson(const std::string& animId, const WcAnimationConfig& animConfig, std::string& errorMsg);

  bool commitTempFile(const std::string& tempPath, const std::string& finalPath);
};

// Global active workspace instance for Aseprite session
WcWorkspace& getActiveWorkspace();

} // namespace wildconquest

#endif

