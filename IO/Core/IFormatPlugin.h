#pragma once
#include <cstdint>

// ── Plugin ABI ────────────────────────────────────────────────────────────────
// Every IO plugin DLL must export exactly these three C symbols.
// Increment XCEL_IO_API_VERSION when the ABI changes.

#define XCEL_IO_API_VERSION 1

#if defined(_WIN32)
  #define XCEL_IO_EXPORT __declspec(dllexport)
#else
  #define XCEL_IO_EXPORT __attribute__((visibility("default")))
#endif

struct XcelPluginInfo
{
    uint32_t    apiVersion;    // must equal XCEL_IO_API_VERSION
    const char* name;          // human-readable, e.g. "Assimp"
    const char* extensions;    // semicolon-separated, lower-case, no dots: "fbx;gltf;glb;obj"
    bool        supportsWrite;
};

namespace xcel::io { class IFormatReader; class IFormatWriter; }

// The three symbols every plugin must export with C linkage:
//
//   XCEL_IO_EXPORT const XcelPluginInfo* xcel_plugin_info();
//   XCEL_IO_EXPORT xcel::io::IFormatReader* xcel_create_reader();
//   XCEL_IO_EXPORT void xcel_destroy_reader(xcel::io::IFormatReader*);
//
// (IFormatWriter equivalents are optional; check XcelPluginInfo::supportsWrite)
//   XCEL_IO_EXPORT xcel::io::IFormatWriter* xcel_create_writer();
//   XCEL_IO_EXPORT void xcel_destroy_writer(xcel::io::IFormatWriter*);

using FnPluginInfo    = const XcelPluginInfo*(*)();
using FnCreateReader  = xcel::io::IFormatReader*(*)();
using FnDestroyReader = void(*)(xcel::io::IFormatReader*);
using FnCreateWriter  = xcel::io::IFormatWriter*(*)();
using FnDestroyWriter = void(*)(xcel::io::IFormatWriter*);
