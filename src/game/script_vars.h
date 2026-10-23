#pragma once

// Reading the game script's variables from guest memory (portal_prompt_watch.cpp).
//
// The game's scripts keep their variables as ScriptReference objects: vtable 0x821095A8, +8 the
// name, +0x1C the value. Names are pooled strings, and the pointers to them are tagged: the
// string's address + 1 ("player state macro" at 0xB7842BF2 is referenced as 0xB7842BF3). A value
// is another script object, which carries its (base's) name at +8 as well. The script heap is in
// the physical memory view 0xA0000000-0xBFFFFFFF; variables of a screen live only while it runs,
// so they are found by a scan for the vtable and the name. Everything here only reads.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <rex/memory/utils.h>
#include <rex/system/xmemory.h>
#include <rex/types.h>

namespace trapteamrecomp::script {

constexpr uint32_t kScriptReferenceVtable = 0x821095A8;
constexpr uint32_t kNameOffset = 0x08;
constexpr uint32_t kValueOffset = 0x1C;
constexpr uint32_t kVariableSize = kValueOffset + 4;
// The view of physical memory the game's heap is in (the other two alias it); 64 KB pages.
constexpr uint32_t kScanStart = 0xA0000000;
constexpr uint32_t kScanEnd = 0xC0000000;
constexpr uint32_t kScanStep = 0x10000;

// How many bytes from `address` on are committed, readable guest memory (0 if none).
inline uint32_t ReadableBytes(rex::memory::Memory* memory, uint32_t address) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead)) {
    return 0;
  }
  // The region is counted in whole pages from the one that holds `address`.
  const uint64_t end = uint64_t(address & ~(heap->page_size() - 1)) + info.region_size;
  return end > address ? uint32_t(std::min<uint64_t>(end - address, UINT32_MAX)) : 0;
}

inline uint32_t ReadU32(rex::memory::Memory* memory, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(memory->TranslateVirtual<const uint8_t*>(address));
}

// The pooled name a tagged pointer refers to; empty if it isn't readable or doesn't end in time.
inline std::string ReadName(rex::memory::Memory* memory, uint32_t tagged) {
  constexpr uint32_t kMaxName = 128;
  const uint32_t address = tagged & ~1u;
  if (!address) return {};
  const uint32_t readable = std::min(ReadableBytes(memory, address), kMaxName);
  if (!readable) return {};
  const char* text = memory->TranslateVirtual<const char*>(address);
  const char* end = static_cast<const char*>(std::memchr(text, 0, readable));
  return end ? std::string(text, end) : std::string();
}

inline std::string ObjectName(rex::memory::Memory* memory, uint32_t object) {
  if (!object || ReadableBytes(memory, object) < kNameOffset + 4) return {};
  return ReadName(memory, ReadU32(memory, object + kNameOffset));
}

inline bool IsVariable(rex::memory::Memory* memory, uint32_t object, std::string_view name) {
  return object && ReadableBytes(memory, object) >= kVariableSize &&
         ReadU32(memory, object) == kScriptReferenceVtable && ObjectName(memory, object) == name;
}

// What the variable at `variable` holds (a script object), 0 if nothing.
inline uint32_t Value(rex::memory::Memory* memory, uint32_t variable) {
  return ReadU32(memory, variable + kValueOffset);
}

// The name of what the script variable at `variable` holds, empty if nothing.
inline std::string ValueName(rex::memory::Memory* memory, uint32_t variable) {
  return ObjectName(memory, Value(memory, variable));
}

// Scans the physical heap for the first variable of each name (0 where there is none). Takes a
// few tens of milliseconds; stops early once every name is found.
inline std::vector<uint32_t> FindVariables(rex::memory::Memory* memory,
                                           const std::vector<std::string_view>& names) {
  std::vector<uint32_t> found(names.size(), 0);
  size_t missing = names.size();
  const uint32_t needle = rex::byte_swap(kScriptReferenceVtable);  // as it is in guest memory
  uint64_t address = kScanStart;
  while (address < kScanEnd && missing) {
    rex::memory::BaseHeap* heap = memory->LookupHeap(uint32_t(address));
    rex::memory::HeapAllocationInfo info{};
    if (!heap || !heap->QueryRegionInfo(uint32_t(address), &info) || !info.region_size) {
      address += kScanStep;
      continue;
    }
    const uint64_t region_end = address + info.region_size;  // `address` is page aligned
    if ((info.state & rex::memory::kMemoryAllocationCommit) &&
        (info.protect & rex::memory::kMemoryProtectRead) && info.region_size >= kVariableSize) {
      const uint8_t* host = memory->TranslateVirtual<const uint8_t*>(uint32_t(address));
      const uint64_t words = (info.region_size - kVariableSize) / 4 + 1;
      for (uint64_t i = 0; i < words && missing; ++i) {
        uint32_t word;
        std::memcpy(&word, host + 4 * i, 4);
        if (word != needle) continue;
        const uint32_t object = uint32_t(address + 4 * i);
        const std::string name = ObjectName(memory, object);
        for (size_t n = 0; n < names.size(); ++n) {
          if (!found[n] && name == names[n]) {
            found[n] = object;
            --missing;
          }
        }
      }
    }
    address = std::max(region_end, address + kScanStep);
  }
  return found;
}

}  // namespace trapteamrecomp::script
