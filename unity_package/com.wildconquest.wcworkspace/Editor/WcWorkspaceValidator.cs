#if UNITY_EDITOR
using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace WildConquest.Pipeline
{
    public static class WcWorkspaceValidator
    {
        [MenuItem("Wild Conquest/Validate Workspace...", false, 10)]
        public static void ValidateWorkspaceDialog()
        {
            string folder = EditorUtility.OpenFolderPanel("Select .wcworkspace Folder", "", "");
            if (string.IsNullOrEmpty(folder)) return;

            List<string> errors = new List<string>();
            List<string> warnings = new List<string>();

            string wsFile = Path.Combine(folder, "workspace.json");
            string charFile = Path.Combine(folder, "source", "character.json");
            string partsDir = Path.Combine(folder, "source", "parts");
            string animsDir = Path.Combine(folder, "animations");

            if (!File.Exists(wsFile)) errors.Add("workspace.json is missing.");
            if (!File.Exists(charFile)) errors.Add("source/character.json is missing.");
            if (!Directory.Exists(partsDir)) errors.Add("source/parts/ folder is missing.");

            int animCount = 0;
            if (Directory.Exists(animsDir))
            {
                string[] subDirs = Directory.GetDirectories(animsDir);
                animCount = subDirs.Length;
                foreach (var dir in subDirs)
                {
                    string animId = Path.GetFileName(dir);
                    string animJson = Path.Combine(dir, "animation.json");
                    if (!File.Exists(animJson)) warnings.Add($"Animation '{animId}' is missing animation.json");
                }
            }

            if (errors.Count == 0)
            {
                string warnStr = warnings.Count > 0 ? $"\nWarnings ({warnings.Count}):\n" + string.Join("\n", warnings) : "";
                EditorUtility.DisplayDialog("Validation Result", $"[VALID] Workspace structure is valid!\nAnimations found: {animCount}{warnStr}", "OK");
            }
            else
            {
                EditorUtility.DisplayDialog("Validation Result", $"[INVALID] Found {errors.Count} error(s):\n" + string.Join("\n", errors), "OK");
            }
        }

        [MenuItem("Wild Conquest/Sync Animation to Aseprite", false, 4)]
        public static void SyncAnimationToAseprite()
        {
            string folder = EditorUtility.OpenFolderPanel("Select .wcworkspace Folder", "", "");
            if (string.IsNullOrEmpty(folder)) return;

            string animsDir = Path.Combine(folder, "animations");
            if (!Directory.Exists(animsDir) || Directory.GetDirectories(animsDir).Length == 0)
            {
                EditorUtility.DisplayDialog("Sync Notice", "No animations found in workspace. Export animation frames first.", "OK");
                return;
            }

            // Open folder in system file explorer for seamless Aseprite access
            EditorUtility.RevealInFinder(animsDir);
            EditorUtility.DisplayDialog("Sync Ready", "Animation frames are ready in workspace. Open Aseprite and select:\n'Wild Conquest -> Sync Animation from Workspace...'", "OK");
        }
    }
}
#endif
