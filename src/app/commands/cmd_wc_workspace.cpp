// Wild Conquest - Shared Workspace Commands (v2 Round-Trip Pipeline)
// Copyright (C) 2026 Wild Conquest Team

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/doc.h"
#include "app/doc_api.h"
#include "app/file_selector.h"
#include "app/tx.h"
#include "app/ui/status_bar.h"
#include "app/wildconquest/wc_workspace.h"
#include "base/fs.h"
#include "doc/layer.h"
#include "doc/sprite.h"
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

    std::string folder;
    if (wildconquest::getActiveWorkspace().isOpen()) {
      folder = wildconquest::getActiveWorkspace().workspaceDir();
    } else {
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
      folder = output.front();
      std::string errorMsg;
      if (!wildconquest::getActiveWorkspace().createNew(folder, docTitle, doc->sprite()->width(), doc->sprite()->height(), errorMsg)) {
        ui::Alert::show(fmt::format("Error creating workspace<<{}||&OK", errorMsg));
        return;
      }
    }

    std::string summary, errorMsg;
    if (!wildconquest::getActiveWorkspace().exportCharacterParts(doc, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Error exporting character parts<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Exported Wild Conquest Workspace: {}", folder));
    }

    ui::Alert::show(fmt::format("Wild Conquest Workspace Exported<<{}||&OK", summary));
  }
};

class SyncAnimFromWorkspaceCommand : public Command {
public:
  SyncAnimFromWorkspaceCommand() : Command("SyncAnimFromWorkspace") {}

protected:
  bool onEnabled(Context* context) override {
    return wildconquest::getActiveWorkspace().isOpen();
  }

  void onExecute(Context* context) override {
    auto& ws = wildconquest::getActiveWorkspace();
    std::vector<std::string> animIds;
    std::string errorMsg;
    if (!ws.getAvailableAnimations(animIds, errorMsg) || animIds.empty()) {
      ui::Alert::show("No Animations Found<<No animation frame folders found in workspace.\nPlease render animation frames in Unity first.||&OK");
      return;
    }

    // Default to the first available animation or inspect current document title
    std::string selectedAnim = animIds.front();
    if (animIds.size() > 1) {
      // Find matching animation if current document has one in name
      const ContextReader reader(context);
      if (reader.document()) {
        std::string title = base::get_file_title(reader.document()->filename());
        for (const auto& id : animIds) {
          if (title.find(id) != std::string::npos) {
            selectedAnim = id;
            break;
          }
        }
      }
    }

    std::string summary;
    if (!ws.syncAnimationToAseprite(selectedAnim, context, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Sync Animation Failed<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Wild Conquest: Synced animation {}", selectedAnim));
    }

    ui::Alert::show(fmt::format("Wild Conquest Sync Animation<<{}||&OK", summary));
  }
};

class ExportPolishedAnimCommand : public Command {
public:
  ExportPolishedAnimCommand() : Command("ExportPolishedAnim") {}

protected:
  bool onEnabled(Context* context) override {
    const ContextReader reader(context);
    return wildconquest::getActiveWorkspace().isOpen() && reader.document() && reader.document()->sprite();
  }

  void onExecute(Context* context) override {
    ContextWriter writer(context);
    Doc* doc = writer.document();
    if (!doc || !doc->sprite()) return;

    auto& ws = wildconquest::getActiveWorkspace();
    std::string docTitle = base::get_file_title(doc->filename());
    std::string animId = docTitle;

    // Check available animations in workspace to find best match
    std::vector<std::string> animIds;
    std::string errorMsg;
    ws.getAvailableAnimations(animIds, errorMsg);
    for (const auto& id : animIds) {
      if (docTitle.find(id) != std::string::npos) {
        animId = id;
        break;
      }
    }
    if (animId.empty() && !animIds.empty()) {
      animId = animIds.front();
    }
    if (animId.empty()) {
      animId = "attack";
    }

    bool overwrite = false;
    fs::path polishedPath = fs::path(ws.workspaceDir()) / "animations" / animId / "polished";
    if (fs::exists(polishedPath) && !fs::is_empty(polishedPath)) {
      int res = ui::Alert::show(fmt::format("Overwrite Protection<<Polished frames already exist for animation '{}'.\nDo you want to replace existing polished artwork?||&Replace||&Cancel", animId));
      if (res != 1) { // 1 is &Replace
        return;
      }
      overwrite = true;
    }

    std::string summary;
    if (!ws.exportPolishedAnimation(doc, animId, overwrite, summary, errorMsg)) {
      ui::Alert::show(fmt::format("Export Polished Failed<<{}||&OK", errorMsg));
      return;
    }

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Wild Conquest: Exported polished animation {}", animId));
    }

    ui::Alert::show(fmt::format("Polished Animation Exported<<{}||&OK", summary));
  }
};

class ValidateWcWorkspaceCommand : public Command {
public:
  ValidateWcWorkspaceCommand() : Command("ValidateWcWorkspace") {}

protected:
  bool onEnabled(Context* context) override {
    return wildconquest::getActiveWorkspace().isOpen();
  }

  void onExecute(Context* context) override {
    auto& ws = wildconquest::getActiveWorkspace();
    std::vector<std::string> errors, warnings;
    bool valid = ws.validate(errors, warnings);

    std::stringstream ss;
    if (valid) {
      ss << "Workspace is VALID!\n\n";
      ss << "Character: " << ws.character().name << "\n";
      ss << "Parts: " << ws.character().parts.size() << "\n";
      if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& w : warnings) ss << "  - " << w << "\n";
      }
    } else {
      ss << "Workspace validation FAILED:\n\n";
      for (const auto& e : errors) ss << "  ✖ " << e << "\n";
      if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& w : warnings) ss << "  ! " << w << "\n";
      }
    }

    ui::Alert::show(fmt::format("Wild Conquest Workspace Validation<<{}||&OK", ss.str()));
  }
};

class CreateWcLayersTemplateCommand : public Command {
public:
  CreateWcLayersTemplateCommand() : Command("CreateWcLayersTemplate") {}

protected:
  void onLoadParams(const Params& params) override {
    m_template = params.get("template");
  }

  bool onEnabled(Context* context) override {
    const ContextReader reader(context);
    return reader.document() && reader.document()->sprite();
  }

  void onExecute(Context* context) override {
    ContextWriter writer(context);
    Doc* doc = writer.document();
    if (!doc || !doc->sprite()) return;

    doc::Sprite* sprite = doc->sprite();
    bool isWinged = (m_template == "winged_humanoid" || m_template == "winged");

    std::vector<std::string> layerNames;
    if (isWinged) {
      layerNames = {
        "wing_l_back", "wing_r_back",
        "arm_l_upper", "arm_l_lower", "hand_l",
        "leg_l_upper", "leg_l_lower", "foot_l",
        "body",
        "head",
        "wing_l_front", "wing_r_front",
        "leg_r_upper", "leg_r_lower", "foot_r",
        "arm_r_upper", "arm_r_lower", "hand_r",
        "shield",
        "weapon"
      };
    } else {
      layerNames = {
        "arm_l_upper", "arm_l_lower", "hand_l",
        "leg_l_upper", "leg_l_lower", "foot_l",
        "body",
        "head",
        "leg_r_upper", "leg_r_lower", "foot_r",
        "arm_r_upper", "arm_r_lower", "hand_r",
        "shield",
        "weapon"
      };
    }

    Tx tx(writer, "Create Wild Conquest Template Layers");
    DocApi api = doc->getApi(tx);

    int count = 0;
    for (const auto& name : layerNames) {
      bool exists = false;
      for (doc::Layer* l : sprite->allLayers()) {
        if (l->name() == name) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        api.newLayer(sprite->root(), name);
        count++;
      }
    }

    tx.commit();

    if (app::StatusBar::instance()) {
      app::StatusBar::instance()->setStatusText(5000, fmt::format("Created {} template layer(s) for {}", count, isWinged ? "Winged Humanoid" : "Humanoid"));
    }
  }

private:
  std::string m_template = "humanoid";
};

Command* CommandFactory::createOpenWcWorkspaceCommand() {
  return new OpenWcWorkspaceCommand;
}

Command* CommandFactory::createSaveAsWcWorkspaceCommand() {
  return new SaveAsWcWorkspaceCommand;
}

Command* CommandFactory::createSyncAnimFromWorkspaceCommand() {
  return new SyncAnimFromWorkspaceCommand;
}

Command* CommandFactory::createExportPolishedAnimCommand() {
  return new ExportPolishedAnimCommand;
}

Command* CommandFactory::createValidateWcWorkspaceCommand() {
  return new ValidateWcWorkspaceCommand;
}

Command* CommandFactory::createCreateWcLayersTemplateCommand() {
  return new CreateWcLayersTemplateCommand;
}

} // namespace app

