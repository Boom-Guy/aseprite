#if UNITY_EDITOR
using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace WildConquest.Pipeline
{
    public static class WcPolishedSpriteSheetImporter
    {
        [MenuItem("Wild Conquest/Import Polished Animation...", false, 3)]
        public static void ImportPolishedAnimationDialog()
        {
            string pngPath = EditorUtility.OpenFilePanel("Select Polished Sprite Sheet", "", "png");
            if (string.IsNullOrEmpty(pngPath)) return;

            ImportPolishedSpriteSheet(pngPath);
        }

        public static bool ImportPolishedSpriteSheet(string spriteSheetFsPath)
        {
            if (!File.Exists(spriteSheetFsPath))
            {
                EditorUtility.DisplayDialog("Error", "Sprite sheet file not found.", "OK");
                return false;
            }

            string animId = Path.GetFileNameWithoutExtension(spriteSheetFsPath);

            // Locate workspace root if within a workspace
            string animFolder = Path.GetDirectoryName(Path.GetDirectoryName(spriteSheetFsPath));
            string animJsonPath = Path.Combine(animFolder, "animation.json");

            int frameWidth = 64;
            int frameHeight = 64;
            int fps = 12;

            if (File.Exists(animJsonPath))
            {
                try
                {
                    WcAnimationConfig animCfg = JsonUtility.FromJson<WcAnimationConfig>(File.ReadAllText(animJsonPath));
                    if (animCfg.canvas.width > 0) frameWidth = animCfg.canvas.width;
                    if (animCfg.canvas.height > 0) frameHeight = animCfg.canvas.height;
                    if (animCfg.fps > 0) fps = animCfg.fps;
                }
                catch (Exception e)
                {
                    Debug.LogWarning($"[WcWorkspace] Could not parse animation.json: {e.Message}");
                }
            }

            string targetDir = $"Assets/WildConquest/PolishedAnimations/{animId}";
            EnsureDirectory(targetDir);

            string destFile = $"{targetDir}/{animId}.png";
            File.Copy(spriteSheetFsPath, destFile, true);
            AssetDatabase.ImportAsset(destFile, ImportAssetOptions.ForceUpdate);

            // Configure Texture Importer as sliced sprite sheet
            TextureImporter importer = AssetImporter.GetAtPath(destFile) as TextureImporter;
            if (importer != null)
            {
                importer.textureType = TextureImporterType.Sprite;
                importer.spriteImportMode = SpriteImportMode.Multiple;
                importer.filterMode = FilterMode.Point;
                importer.textureCompression = TextureImporterCompression.Uncompressed;
                importer.mipmapEnabled = false;
                importer.wrapMode = TextureWrapMode.Clamp;
                importer.isReadable = true;

                // Determine dimensions from actual texture
                Texture2D tex = AssetDatabase.LoadAssetAtPath<Texture2D>(destFile);
                int totalWidth = tex != null ? tex.width : frameWidth;
                int totalHeight = tex != null ? tex.height : frameHeight;
                int cols = Mathf.Max(1, totalWidth / frameWidth);

                List<SpriteMetaData> metaDataList = new List<SpriteMetaData>();
                for (int i = 0; i < cols; i++)
                {
                    SpriteMetaData smd = new SpriteMetaData
                    {
                        name = $"{animId}_{i:D3}",
                        rect = new Rect(i * frameWidth, 0, frameWidth, frameHeight),
                        alignment = (int)SpriteAlignment.Center,
                        pivot = new Vector2(0.5f, 0.5f)
                    };
                    metaDataList.Add(smd);
                }

                importer.spritesheet = metaDataList.ToArray();
                EditorUtility.SetDirty(importer);
                importer.SaveAndReimport();
            }

            // Create or update pure Sprite Animation Clip (without touching any Rig/Bones)
            UnityEngine.Object[] assets = AssetDatabase.LoadAllAssetsAtPath(destFile);
            List<Sprite> sprites = new List<Sprite>();
            foreach (var a in assets)
            {
                if (a is Sprite spr) sprites.Add(spr);
            }
            sprites.Sort((a, b) => string.Compare(a.name, b.name, StringComparison.Ordinal));

            if (sprites.Count > 0)
            {
                string clipPath = $"{targetDir}/{animId}_Polished.anim";
                AnimationClip clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(clipPath);
                if (clip == null)
                {
                    clip = new AnimationClip();
                    AssetDatabase.CreateAsset(clip, clipPath);
                }

                clip.frameRate = fps;

                EditorCurveBinding spriteBinding = new EditorCurveBinding
                {
                    type = typeof(SpriteRenderer),
                    path = "",
                    propertyName = "m_Sprite"
                };

                ObjectReferenceKeyframe[] keyframes = new ObjectReferenceKeyframe[sprites.Count];
                for (int i = 0; i < sprites.Count; i++)
                {
                    keyframes[i] = new ObjectReferenceKeyframe
                    {
                        time = (float)i / (float)fps,
                        value = sprites[i]
                    };
                }

                AnimationUtility.SetObjectReferenceCurve(clip, spriteBinding, keyframes);
                EditorUtility.SetDirty(clip);
                AssetDatabase.SaveAssets();

                Debug.Log($"[WcWorkspace] Polished Animation '{animId}' imported safely. Rigs, Bones, and Hierarchy remain 100% intact.");
                EditorUtility.DisplayDialog("Polished Animation Imported", $"Successfully imported '{animId}' with {sprites.Count} frames ({fps} FPS).\nGenerated Animation Clip: {clipPath}\n\nRig, skeleton, and Animator intact.", "OK");
                return true;
            }

            return false;
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

