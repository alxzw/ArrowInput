#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <msctf.h>
#include <shlobj.h>
#include <strsafe.h>

#include <atomic>
#include <new>
#include <string>
#include <vector>

namespace arrowinput {

// {976A6FC6-B6A9-4B1D-A7C3-E8CE14A60274}
inline constexpr GUID kClsidTextService =
    {0x976a6fc6, 0xb6a9, 0x4b1d, {0xa7, 0xc3, 0xe8, 0xce, 0x14, 0xa6, 0x02, 0x74}};

// {2DDC4D1E-8464-431B-9D60-F9DD59F6F818}
inline constexpr GUID kProfileGuid =
    {0x2ddc4d1e, 0x8464, 0x431b, {0x9d, 0x60, 0xf9, 0xdd, 0x59, 0xf6, 0xf8, 0x18}};

// {7E960D7B-4F33-4879-B245-A83E6D611716}
inline constexpr GUID kPreeditDisplayAttributeGuid =
    {0x7e960d7b, 0x4f33, 0x4879, {0xb2, 0x45, 0xa8, 0x3e, 0x6d, 0x61, 0x17, 0x16}};

// {7B7B8F64-81E7-4A11-9B18-2AA70D941598}
inline constexpr GUID kFullShapePreservedKeyGuid =
    {0x7b7b8f64, 0x81e7, 0x4a11, {0x9b, 0x18, 0x2a, 0xa7, 0x0d, 0x94, 0x15, 0x98}};

// {A652645E-F95B-49E2-8E09-612CD6E1D782}
inline constexpr GUID kFullShapeLeftShiftPreservedKeyGuid =
    {0xa652645e, 0xf95b, 0x49e2, {0x8e, 0x09, 0x61, 0x2c, 0xd6, 0xe1, 0xd7, 0x82}};

// {E2B87C9C-4A5E-4710-842B-F6C63AFD147E}
inline constexpr GUID kFullShapeRightShiftPreservedKeyGuid =
    {0xe2b87c9c, 0x4a5e, 0x4710, {0x84, 0x2b, 0xf6, 0xc6, 0x3a, 0xfd, 0x14, 0x7e}};

inline constexpr wchar_t kTextServiceDescription[] = L"ArrowInput";
inline constexpr wchar_t kConfigDirectoryName[] = L"ArrowInput";
inline constexpr wchar_t kConfigFileName[] = L"config.ini";

extern HINSTANCE g_module;
extern std::atomic<long> g_object_count;
extern std::atomic<long> g_lock_count;

void AddRefServer();
void ReleaseServer();

}  // namespace arrowinput
