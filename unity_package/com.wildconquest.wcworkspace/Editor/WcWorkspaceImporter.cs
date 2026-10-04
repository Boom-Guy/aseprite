#if UNITY_EDITOR
using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace WildConquest.Pipeline
{
    public static class WcWorkspaceImporter
    {
        private const string MenuRoot = "Wild Conquest/";

        [MenuItem(MenuRoot + "Import Workspace...", false, 1)]
        public static void ImportWorkspaceDialog()
        {
            string folder = EditorUtility.OpenFolderPanel("Select .wcworkspace Folder", "", "");
            if (string.IsNullOrEmpty(folder)) return;

            ImportWorkspaceFromPath(folder);
        }

        public static bool ImportWorkspaceFromPath(string workspacePath)
        {
            string wsFile = Path.Combine(workspacePath, "workspace.json");
            string charFile = Path.Combine(workspacePath, "source", "character.json");

            if (!File.Exists(wsFile) || !File.Exists(charFile))
            {
                EditorUtility.DisplayDialog("Import Error", "Invalid .wcworkspace. Missing workspace.json or source/character.json.", "OK");
                return false;
            }

            WcWorkspaceConfig wsConfig;
            WcCharacterData charData;

            try
            {
                wsConfig = JsonUtility.FromJson<WcWorkspaceConfig>(File.ReadAllText(wsFile));
                charData = JsonUtility.FromJson<WcCharacterData>(File.ReadAllText(charFile));
            }
            catch (Exception ex)
            {
                EditorUtility.DisplayDialog("Parse Error", $"Failed to parse workspace metadata: {ex.Message}", "OK");
                return false;
            }

            string characterName = string.IsNullOrEmpty(charData.name) ? "Character" : charData.name;
            string targetFolder = $"Assets/WildConquest/Characters/{charData.id}";
            EnsureDirectory(targetFolder);

            // Copy parts to Unity Project
            string partsSourceDir = Path.Combine(workspacePath, "source", "parts");
            string partsTargetDir = Path.Combine(targetFolder, "Parts");
            EnsureDirectory(partsTargetDir);

            int canvasW = wsConfig.canvas.width > 0 ? wsConfig.canvas.width : 64;
            int canvasH = wsConfig.canvas.height > 0 ? wsConfig.canvas.height : 64;

            Dictionary<string, Sprite> importedSprites = new Dictionary<string, Sprite>();

            AssetDatabase.StartAssetEditing();
            try
            {
                foreach (var part in charData.parts)
                {
                    string srcPartPng = Path.Combine(workspacePath, part.asset);
                    if (!File.Exists(srcPartPng))
                    {
                        Debug.LogWarning($"[WcWorkspace] Part asset not found: {srcPartPng}");
                        continue;
                    }

                    string destFile = Path.Combine(partsTargetDir, Path.GetFileName(srcPartPng));
                    File.Copy(srcPartPng, destFile, true);
                }
            }
            finally
            {
                AssetDatabase.StopAssetEditing();
            }

            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);

            // Configure Texture Importers pixel-perfect
            foreach (var part in charData.parts)
            {
                string partFileName = Path.GetFileName(part.asset);
                string assetPath = $"{partsTargetDir}/{partFileName}";
                TextureImporter importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;

                if (importer != null)
                {
                    importer.textureType = TextureImporterType.Sprite;
                    importer.spriteImportMode = SpriteImportMode.Single;
                    importer.filterMode = FilterMode.Point;
                    importer.textureCompression = TextureImporterCompression.Uncompressed;
                    importer.mipmapEnabled = false;
                    importer.wrapMode = TextureWrapMode.Clamp;
                    importer.isReadable = true;

                    // Set pixel-perfect pivot
                    float pivotX = part.pivot.x / Mathf.Max(1f, canvasW);
                    float pivotY = 1.0f - (part.pivot.y / Mathf.Max(1f, canvasH)); // Invert Y for Unity coordinate space
                    importer.spritePivot = new Vector2(Mathf.Clamp01(pivotX), Mathf.Clamp01(pivotY));

                    TextureImporterSettings settings = new TextureImporterSettings();
                    importer.ReadTextureSettings(settings);
                    settings.spriteAlignment = (int)SpriteAlignment.Custom;
                    importer.SetTextureSettings(settings);

                    EditorUtility.SetDirty(importer);
                    importer.SaveAndReimport();
                }

                Sprite spr = AssetDatabase.LoadAssetAtPath<Sprite>(assetPath);
                if (spr != null)
                {
                    importedSprites[part.id] = spr;
                }
            }

            // Create Character Hierarchy in Scene
            GameObject charRoot = new GameObject(characterName);
            Undo.RegisterCreatedObjectUndo(charRoot, "Create Wild Conquest Character");

            GameObject partsRoot = new GameObject("Parts");
            partsRoot.transform.SetParent(charRoot.transform, false);

            foreach (var part in charData.parts)
            {
                GameObject partObj = new GameObject(string.IsNullOrEmpty(part.name) ? part.id : part.name);
                partObj.transform.SetParent(partsRoot.transform, false);

                SpriteRenderer sr = partObj.AddComponent<SpriteRenderer>();
                if (importedSprites.TryGetValue(part.id, out Sprite spr))
                {
                    sr.sprite = spr;
                }
                sr.sortingOrder = part.zIndex;
                partObj.SetActive(part.visible);
            }

            Selection.activeGameObject = charRoot;
            EditorUtility.DisplayDialog("Workspace Imported", $"Character '{characterName}' imported successfully with {charData.parts.Count} parts.\nReady for 2D Animation and Rigging in Unity.", "OK");
            return true;
        }

        private static void EnsureDirectory(string relativeAssetPath)
        {
            if (!AssetDatabase.IsValidFolder(relativeAssetPath))
            {
                string[] parts = relativeAssetPath.Split('/');
                string current = parts[0];
                for (int i = 1; i < parts.Length; i++)
                {
                    string next = $"{current}/{parts[i]}";
                    if (!AssetDatabase.IsValidFolder(next))
                    {
                        AssetDatabase.CreateFolder(current, parts[i]);
                    }
                    current = next;
                }
            }
        }
    }
}
#endif

