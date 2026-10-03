// Wild Conquest - Shared Workspace Module
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
  std::string asset; // e.g. "parts/head.png"
  WcVec2 pivot{0.0, 0.0};
  int zIndex = 0;
  bool visible = true;
};

struct WcWorkspaceManifest {
  std::string format = "wcworkspace";
  int version = 1;
  std::string workspaceId;
  std::string name;
  std::string character;
  WcCanvas canvas{64, 64};
  int pixelScale = 1;
  std::string facing = "right";
  int groundY = 59;
  std::vector<WcPart> parts;
};

struct WcPartSyncInfo {
  int version = 1;
  std::string hash;
  std::string source = "aseprite";
};

struct WcAnimSyncInfo {
  int version = 1;
  std::string source = "skelform";
  int frameCount = 0;
  std::string hash;
};

struct WcSyncManifest {
  int version = 1;
  std::string lastSyncTime;
  std::map<std::string, WcPartSyncInfo> parts;
  std::map<std::string, WcAnimSyncInfo> animations;
};

class WcWorkspace {
public:
  WcWorkspace();
  ~WcWorkspace();

  bool isOpen() const { return !m_workspaceDir.empty(); }
  const std::string& workspaceDir() const { return m_workspaceDir; }
  const WcWorkspaceManifest& manifest() const { return m_manifest; }
  const WcSyncManifest& syncManifest() const { return m_sync; }

  // File I/O
  bool open(const std::string& dirPath, std::string& errorMsg);
  bool save(std::string& errorMsg);
  bool createNew(const std::string& dirPath, const std::string& characterName, int width, int height, std::string& errorMsg);

  // Sync operations from Aseprite
  bool exportLayersToWorkspace(app::Doc* doc, std::string& outSummary, std::string& errorMsg);
  bool syncToSkelForm(app::Doc* doc, std::string& outSummary, std::string& errorMsg);
  bool refreshFromWorkspace(app::Doc* doc, app::Context* ctx, std::string& outSummary, std::string& errorMsg);

  // Helper for computing file hash
  static std::string calculateFileHash(const std::string& filePath);
  static std::string calculateBufferHash(const uint8_t* data, size_t size);

private:
  std::string m_workspaceDir;
  WcWorkspaceManifest m_manifest;
  WcSyncManifest m_sync;

  bool loadManifest(std::string& errorMsg);
  bool saveManifest(std::string& errorMsg);
  bool loadSyncManifest(std::string& errorMsg);
  bool saveSyncManifest(std::string& errorMsg);

  // Atomic writing helpers
  bool commitTempFile(const std::string& tempPath, const std::string& finalPath);
};

// Global active workspace instance for Aseprite session
WcWorkspace& getActiveWorkspace();

} // namespace wildconquest

#endif
