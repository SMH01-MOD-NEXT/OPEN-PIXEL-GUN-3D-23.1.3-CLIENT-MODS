// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Run 49 proved the boot progress answer reaches the client intact and is
// still followed, four milliseconds later, by
//
//     NullReferenceException
//       at Progress.<applier> (System.Collections.Generic.Dictionary`2)
//       at AuthSceneController+<coroutine>[T].MoveNext ()
//
// The consumer at RVA 0x17AB704 reads three string literals out of the answer
// it is given: the presence of the first turns the answer into err_code 1602,
// the presence of the second ends it, and the value of the third is cast to
// Dictionary<string, object> and handed to the applier at 0x2701830. A cast
// that fails yields null and the applier throws on null, so the exception
// reads the same whether the third key carries the wrong type or the applier
// trips over the contents of a document it did receive.
//
// This trace separates those two cases and changes no value:
//   * the applier reports whether it was handed a document or a null, which
//     confirms or rules out the failing cast in a single run;
//   * the three literal cells are read at applier entry, by which point the
//     consumer has filled them, so the keys are named instead of guessed.
//     Reading them any earlier prints uninitialised memory (run 41).
namespace progress_apply_trace_2313 {
namespace detail {

using MethodInfo = void;

constexpr int32_t kMaxReports = 4;

struct LiteralCell {
    uintptr_t offset;
    const char* what;
};

inline std::atomic<bool> g_cells_named{false};

// A metadata-usage slot holds an encoded index until the runtime resolves it
// on first execution, and only then the address of the cell that holds the
// object. Run 50 read one that was still an index, 0xA000DF0F -- the 0xA0000000
// marker is the string-literal usage type -- and dereferencing it killed the
// process, exactly as run 40b did. So every hop is checked before it is taken.
inline bool readable(const void* address, size_t length) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    if (value < 0x100000u) {
        return false;
    }
    // Managed pointers are eight-byte aligned; encoded indices rarely are, and
    // this catches them before mincore() is asked about a nonsense page.
    if ((value & 0x7u) != 0u) {
        return false;
    }
    const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    if (page == 0u) {
        return false;
    }
    const uintptr_t first = value & ~(page - 1u);
    const size_t span = static_cast<size_t>(value + length - first);
    unsigned char resident[8];
    if ((span + page - 1u) / page > sizeof(resident)) {
        return false;
    }
    return mincore(reinterpret_cast<void*>(first), span, resident) == 0;
}

inline std::string class_name(void* object);

// Names as many of the three cells as are resolved, and latches only once all
// of them are, so a call that arrives before the consumer has filled them is
// reported and then retried on the next one instead of being taken as final.
inline void name_literals() {
    if (g_cells_named.load(std::memory_order_relaxed)) {
        return;
    }
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        return;
    }
    static constexpr LiteralCell kCells[] = {
        {0x680DAC8, "key whose presence sets err_code to 1602"},
        {0x67DB910, "key whose presence ends the answer"},
        {0x6790A50, "key whose value is cast to the applier's dictionary"},
    };
    int named = 0;
    for (const LiteralCell& cell : kCells) {
        const void* slot_address = reinterpret_cast<const void*>(base + cell.offset);
        if (!readable(slot_address, sizeof(void*))) {
            continue;
        }
        void** slot = *reinterpret_cast<void***>(base + cell.offset);
        if (!readable(slot, sizeof(void*))) {
            LOGI("23.1.3-apply: %s is not resolved yet (cell holds %p)",
                 cell.what, static_cast<void*>(slot));
            continue;
        }
        void* value = *slot;
        if (!readable(value, 0x20)) {
            continue;
        }
        const std::string klass = class_name(value);
        if (klass != "String") {
            LOGI("23.1.3-apply: %s holds a %s, not a string", cell.what, klass.c_str());
            continue;
        }
        const std::string text = il2cpp::to_utf8(value, 128);
        if (text.empty()) {
            continue;
        }
        ++named;
        LOGI("23.1.3-apply: %s = \"%s\"", cell.what, text.c_str());
    }
    if (named == 3) {
        g_cells_named.store(true, std::memory_order_relaxed);
    }
}

inline std::string class_name(void* object) {
    if (object == nullptr) {
        return std::string("<null>");
    }
    if (il2cpp::object_get_class == nullptr || il2cpp::class_get_name == nullptr) {
        return std::string("<no reflection>");
    }
    void* klass = il2cpp::object_get_class(object);
    if (klass == nullptr) {
        return std::string("<no class>");
    }
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr ? std::string(name) : std::string("<unnamed>");
}

// The socket dispatch trace already reports the element count of these same
// objects from +0x20, so the offset is reused rather than reasoned about here.
inline int32_t count_at_0x20(void* object) {
    if (object == nullptr) {
        return -1;
    }
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(object) + 0x20);
}

using Consumer = void (*)(void*, void*, void*, const MethodInfo*);
inline Consumer g_consumer = nullptr;
inline std::atomic<int32_t> g_consumer_calls;

void consumer(void* head, void* args, void* attachments, const MethodInfo* method) {
    const int32_t call = g_consumer_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        // Everything is reported before the original runs, because the original
        // reaches the applier and the applier is where the exception leaves
        // this frame for good.
        const std::string head_class = class_name(head);
        const std::string args_class = class_name(args);
        const std::string attachments_class = class_name(attachments);
        LOGI("23.1.3-apply: #%d consumer head=%p %s count@0x20=%d args=%p %s "
             "count@0x20=%d attachments=%p %s count@0x20=%d",
             call, head, head_class.c_str(), count_at_0x20(head),
             args, args_class.c_str(), count_at_0x20(args),
             attachments, attachments_class.c_str(), count_at_0x20(attachments));
    }
    if (g_consumer != nullptr) {
        g_consumer(head, args, attachments, method);
    }
}

using Applier = void (*)(void*, const MethodInfo*);
inline Applier g_applier = nullptr;
inline std::atomic<int32_t> g_applier_calls;

void applier(void* document, const MethodInfo* method) {
    const int32_t call = g_applier_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        // Reported first and separately: this one line decides whether the
        // consumer's cast failed, and it must survive anything that follows.
        const std::string document_class = class_name(document);
        LOGI("23.1.3-apply: #%d applier was handed %s document=%p %s count@0x20=%d",
             call, document != nullptr ? "a document" : "a null", document,
             document_class.c_str(), document != nullptr ? count_at_0x20(document) : -1);
        name_literals();
    }
    if (g_applier != nullptr) {
        g_applier(document, method);
    }
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    armed += hook::install({"Progress", u8"专世丁丆丌丑丅丁丟", u8"丙丈万东下丒丝丞七", 3},
                           reinterpret_cast<void*>(&detail::consumer),
                           reinterpret_cast<void**>(&detail::g_consumer), false) ? 1 : 0;  // 0x17AB704
    armed += hook::install({"Progress", u8"与丅丟丈丕上东丟丁", u8"一下丌丝丕丕上丈丘", 1},
                           reinterpret_cast<void*>(&detail::applier),
                           reinterpret_cast<void**>(&detail::g_applier), false) ? 1 : 0;  // 0x2701830
    LOGI("23.1.3-apply: boot progress applier trace armed on %d/2 entry point(s); "
         "passive passthrough", armed);
    return armed > 0;
}

} // namespace progress_apply_trace_2313
