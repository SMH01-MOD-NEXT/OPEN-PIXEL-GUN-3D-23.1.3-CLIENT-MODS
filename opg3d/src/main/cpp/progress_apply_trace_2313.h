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

// The boot dies four milliseconds after the progress answer arrives:
//
//     NullReferenceException
//       at Progress.<applier> (System.Collections.Generic.Dictionary`2)
//       at AuthSceneController+<coroutine>[T].MoveNext ()
//
// Runs 50 to 52 hooked a second Progress method as the presumed caller and it
// did not execute once, so the literals it reads named nothing on this path.
// The stack trace was exact all along: the applier has only two callers, that
// method and the coroutine's own MoveNext, and the return address the applier
// reports, libil2cpp.so+0x3E753B8, is the instruction after the call at
// 0x3E753B4 inside MoveNext. MoveNext awaits the request, takes the response
// object the task produces, and hands the applier that object's member at
// +0x18. The applier receives a null, so the member is empty, not merely of
// the wrong shape.
//
// Runs 52 and 53 answered under two different member names and the applier
// still received a null, which ruled the name out as the whole story. The
// reply reader was then read out of the image: the request keeps the document
// in a field of its own at +0x18, and the coroutine copies that field into the
// response it hands the applier. The field is filled in one place only, the
// reader at 0x17A8B40, which looks the answer up by a single key and casts it
// to Dictionary<string, object>, keeping a null when the cast does not hold.
// So the null is produced before the coroutine ever runs, and the two things
// still unknown are that key's name and the type our answer decodes to. Both
// are read here: the reader is hooked to list the reply's members with their
// runtime classes, and the two cells it loads are named once it has run.
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
    // The only two cells the reply reader loads. The first names the member it
    // casts to the document, the second the member it reads the error code
    // from; a sibling class shares the second cell and run 51 already resolved
    // it to "err_code", which is what makes it a control for this reading.
    static constexpr LiteralCell kCells[] = {
        {0x67DE2D8, "key holding the progress document"},
        {0x67DB910, "key holding the error code"},
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
            ++named;
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
    if (named == static_cast<int>(sizeof(kCells) / sizeof(kCells[0]))) {
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
    // A metadata cell can hold something that is not a managed object at all,
    // and run 53 killed the process by handing one to the runtime to name. The
    // class pointer is checked exactly like the object was, before it is
    // followed.
    if (!readable(*reinterpret_cast<void* const*>(object), 0x100)) {
        return std::string("<not an object>");
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

// Dictionary<K, V> keeps its entry array at +0x18 and its live count at +0x20,
// and an Entry is {int hashCode; int next; K key; V value}, so the entries
// begin one array header, 0x20, into the array object.
inline void report_head_entries(void* dictionary) {
    if (!readable(dictionary, 0x28)) {
        return;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(dictionary);
    void* entries = *reinterpret_cast<void**>(base + 0x18);
    const int32_t count = *reinterpret_cast<int32_t*>(base + 0x20);
    if (count <= 0 || count > 32 || !readable(entries, 0x20)) {
        return;
    }
    const uintptr_t payload = reinterpret_cast<uintptr_t>(entries) + 0x20;
    for (int32_t index = 0; index < count; ++index) {
        const uintptr_t entry = payload + static_cast<uintptr_t>(index) * 24u;
        if (!readable(reinterpret_cast<void*>(entry), 24)) {
            return;
        }
        void* key = *reinterpret_cast<void**>(entry + 8);
        void* value = *reinterpret_cast<void**>(entry + 16);
        const std::string key_text =
            readable(key, 0x20) ? il2cpp::to_utf8(key, 64) : std::string();
        LOGI("23.1.3-head:   \"%s\" holds a %s", key_text.c_str(),
             class_name(value).c_str());
    }
}

using CodeReader = int32_t (*)(void*, void*, const MethodInfo*);
inline CodeReader g_code_reader = nullptr;
inline std::atomic<int32_t> g_code_reader_calls;

// The reply reader decides everything: it stores the cast result at +0x18 of
// the request, and that is the object the applier is handed a moment later.
// Reporting the members before it runs and the stored field after it separates
// a key we never send from an answer whose shape the cast rejects.
int32_t code_reader(void* self, void* head, const MethodInfo* method) {
    const int32_t call = g_code_reader_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-head: #%d reply head=%p %s count@0x20=%d", call, head,
             class_name(head).c_str(), count_at_0x20(head));
        report_head_entries(head);
    }
    const int32_t code = g_code_reader != nullptr ? g_code_reader(self, head, method) : 0;
    if (call <= kMaxReports) {
        void* document =
            readable(self, 0x28)
                ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x18)
                : nullptr;
        LOGI("23.1.3-head: #%d code=%d document=%p %s count@0x20=%d", call, code,
             document, class_name(document).c_str(),
             document != nullptr ? count_at_0x20(document) : -1);
        name_literals();
    }
    return code;
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
    armed += hook::install({"Progress", u8"丄丄丘不丛专丙丈丈", u8"丁丌丄七丘三丘丛丂", 1},
                           reinterpret_cast<void*>(&detail::code_reader),
                           reinterpret_cast<void**>(&detail::g_code_reader), false) ? 1 : 0;  // 0x17A8B40
    LOGI("23.1.3-apply: boot progress applier trace armed on %d/3 entry point(s); "
         "passive passthrough", armed);
    return armed > 0;
}

} // namespace progress_apply_trace_2313
