### IO (`IO/`)

Async mesh/scene/animation IO layer. Sits between file/application sources and the Graphics `World`.

#### Layers

| Folder | Purpose |
|---|---|
| `Core/` | `IOManager` (async scheduler), `FormatRegistry`, `SceneBuilder`, stream abstractions, plugin ABI |
| `Scene/` | `SceneDocument` (TOC + in-memory data), `SceneNode`, `MeshData`, `SkeletonData`, `ChunkDescriptor` |
| `Animation/` | `AnimationCatalogue` (TOC metadata), `AnimationStream` (demand-paged frames), `FrameCache` (LRU) |
| `Formats/` | `XcelFormat` (built-in native .xcel), plugin subdirectories |

#### Threading model

`IOManager::LoadAsync(path, pool)` is non-blocking. It submits a task to the existing `ThreadPool`; the main thread calls `Poll()` each frame to drain completed documents. Readers may fan out sub-tasks to the same pool (join before returning).

#### Adding a new format

1. Implement `IFormatReader` / `IFormatWriter`.
2. Export the three C-ABI symbols from `IFormatPlugin.h`.
3. Build as a `SHARED` target named `XcelIO_<YourFormat>`.
4. Drop the DLL alongside `XcelViewer.exe`; `ScanPluginDir(".")` auto-discovers it.

#### Native .xcel binary layout

```
[header]  magic("XCEL") + version(u32) + tocOffset(u64)
[chunks]  MeshChunk | SkeletonChunk | AnimCatalogueChunk | AnimFrameChunk  (8-byte aligned)
[TOC]     count(u32) + TOCEntry[N]  { type(u8) id(u32) flags(u32) offset(u64) size(u64) name }
[trailer] tocOffset(u64)   (patched after streaming write)
```

Animation frames are stored at seekable offsets; `AnimationStream::GetFrame()` decodes on demand without loading the whole file.
