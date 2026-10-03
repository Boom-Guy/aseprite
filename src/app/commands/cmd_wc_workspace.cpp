// Wild Conquest - Shared Workspace Commands
// Copyright (C) 2026 Wild Conquest Team

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/doc.h"
#include "app/file_selector.h"
#include "app/ui/status_bar.h"
#include "app/wildconquest/wc_workspace.h"
#include "base/fs.h"
#include "fmt/format.h"
#include "ui/alert.h"
#include "ui/window.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace app {

class OpenWcWorkspaceCommand : public Command {
public:
  OpenWcWorkspaceCommand() : Command("OpenWcWorkspace") {}

protected:
  bool onEnabled(Context* context) override {
    return context->isUIAvailable();
  }

  void onExecute(Context* context) override {
    std::string initialPath;
    if (wildconquest::getActiveWorkspace().isOpen()) {
      initialPath = wildconquest::getActiveWorkspace().workspaceDir();
    }

    base::paths exts = {"wcworkspace"};
    base::paths output;
    if (!app::show_file_selector(
          "Open Wild Conquest Workspace",
          initialPath,
          exts,
          FileSelectorType::Open,
          output) || output.empty()) {
      return;
    }

    std::string folder = output.front();
    std::string errorMsg;
    if (!wildconquest::getActiveWorkspace().open(folder, errorMsg)) {
      ui::Alert::show(fmt::format("Error opening Wild Conquest Workspace<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Opened Wild Conquest Workspace: {}", folder));
    }

    ui::Alert::show(fmt::format("Wild Conquest Workspace<<Opened successfully: {}||&OK", folder));
  }
};

class SaveAsWcWorkspaceCommand : public Command {
public:
  SaveAsWcWorkspaceCommand() : Command("SaveAsWcWorkspace") {}

protected:
  bool onEnabled(Context* context) override {
    const ContextReader reader(context);
    return reader.document() && reader.document()->sprite();
  }

  void onExecute(Context* context) override {
    ContextWriter writer(context);
    Doc* doc = writer.document();
    if (!doc || !doc->sprite()) return;

    std::string docTitle = doc->filename();
    if (docTitle.empty()) docTitle = "Character";
    else docTitle = base::get_file_title(docTitle);

    base::paths exts = {"wcworkspace"};
    base::paths output;
    if (!app::show_file_selector(
          "Save as Wild Conquest Workspace (.wcworkspace)",
          docTitle + ".wcworkspace",
          exts,
          FileSelectorType::Save,
          output) || output.empty()) {
      return;
    }

    std::string folder = output.front();
    std::string errorMsg;
    if (!wildconquest::getActiveWorkspace().createNew(folder, docTitle, doc->sprite()->width(), doc->sprite()->height(), errorMsg)) {
      ui::Alert::show(fmt::format("Error creating workspace<<{}||&OK", errorMsg));
      return;
    }

    std::string summary;
    if (!wildconquest::getActiveWorkspace().exportLayersToWorkspace(doc, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Error exporting layers to workspace<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Saved Wild Conquest Workspace: {}", folder));
    }

    ui::Alert::show(fmt::format("Wild Conquest Workspace Saved<<{}||&OK", summary));
  }
};

class SyncToSkelFormCommand : public Command {
public:
  SyncToSkelFormCommand() : Command("SyncToSkelForm") {}

protected:
  bool onEnabled(Context* context) override {
    const ContextReader reader(context);
    return wildconquest::getActiveWorkspace().isOpen() && reader.document() && reader.document()->sprite();
  }

  void onExecute(Context* context) override {
    ContextWriter writer(context);
    Doc* doc = writer.document();
    if (!doc) return;

    std::string summary, errorMsg;
    if (!wildconquest::getActiveWorkspace().syncToSkelForm(doc, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Sync Failed<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, "Wild Conquest: Synced to SkelForm");
    }

    ui::Alert::show(fmt::format("Wild Conquest Sync<<{}||&OK", summary));
  }
};

class RefreshFromWcWorkspaceCommand : public Command {
public:
  RefreshFromWcWorkspaceCommand() : Command("RefreshFromWcWorkspace") {}

protected:
  bool onEnabled(Context* context) override {
    const ContextReader reader(context);
    return wildconquest::getActiveWorkspace().isOpen() && reader.document();
  }

  void onExecute(Context* context) override {
    ContextWriter writer(context);
    Doc* doc = writer.document();
    if (!doc) return;

    std::string summary, errorMsg;
    if (!wildconquest::getActiveWorkspace().refreshFromWorkspace(doc, context, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Refresh Failed<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, "Wild Conquest: Refreshed from Workspace");
    }

    ui::Alert::show(fmt::format("Wild Conquest Refresh<<{}||&OK", summary));
  }
};

Command* CommandFactory::createOpenWcWorkspaceCommand() {
  return new OpenWcWorkspaceCommand;
}

Command* CommandFactory::createSaveAsWcWorkspaceCommand() {
  return new SaveAsWcWorkspaceCommand;
}

Command* CommandFactory::createSyncToSkelFormCommand() {
  return new SyncToSkelFormCommand;
}

Command* CommandFactory::createRefreshFromWcWorkspaceCommand() {
  return new RefreshFromWcWorkspaceCommand;
}

} // namespace app
