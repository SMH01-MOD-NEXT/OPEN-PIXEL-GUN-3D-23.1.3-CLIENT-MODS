#include <cassert>
#include "../opg3d/src/main/cpp/admin_resync_2313.h"
namespace il2cpp {
void* find_class(const char*, const char*) { return nullptr; }
void* find_method_by_address(const char*, const char*, void*) { return nullptr; }
void* method_pointer(void*) { return nullptr; }
std::string to_utf8(void*, size_t) { return {}; }
}
namespace hook { bool install(const ManagedMethod&, void*, void**, bool) { return false; } }
namespace elfsym { bool find_library(const char*, uintptr_t*) { return false; } }
int menu_calls=0, update_calls=0, reply_calls=0, load_calls=0;
void menu(void*, const void*) { ++menu_calls; }
void tick(void*, const void*) { ++update_calls; }
void answer(void*, void*, void*, const void*) { ++reply_calls; }
int scene_token;
void* scene_new(const char* name) { assert(std::string(name)=="AuthorizationScene"); return &scene_token; }
void* load(void* scene,int mode,const void*) { assert(scene==&scene_token && mode==0);++load_calls;return &scene_token; }
int main() {
 using namespace admin_resync_2313::detail;
 assert(parse_revision("1")==1 && parse_revision("18446744073709551615")==UINT64_MAX);
 for (auto s : {"", "0", "01", "-1", "1.2", "1x", "18446744073709551616", "123456789012345678901"}) assert(parse_revision(s)==0);
 original_menu_enable=menu;original_update=tick;original_reply=answer;load_scene=load;il2cpp::string_new=scene_new;
 request({"player-a",1});update(nullptr,nullptr);assert(load_calls==0); // Cold boot owns auth.
 menu_enable(nullptr,nullptr);assert(menu_calls==1);
 request({"player-a",2});request({"player-a",1});assert(load_calls==0); // Never load in socket callback.
 update(nullptr,nullptr);assert(load_calls==1 && started_revisions["player-a"]==2 && !boot_completed);
 request({"player-a",3});update(nullptr,nullptr);assert(load_calls==1); // No reload during boot.
 menu_enable(nullptr,nullptr);request({"player-a",2});update(nullptr,nullptr);assert(load_calls==1); // Replay.
 request({"player-a",3});request({"player-a",4});update(nullptr,nullptr);assert(load_calls==2 && started_revisions["player-a"]==4);
 reply(nullptr,nullptr,nullptr,nullptr);assert(reply_calls==1); // Malformed/missing marker preserves stock.
 menu_enable(nullptr,nullptr);request({"player-a",5});load_scene=nullptr;update(nullptr,nullptr);assert(load_calls==2);
 assert(parse_marker("678095418:25").revision==25);
 for (auto s : {"1", ":1", "a:", "a:0", "a:1:2", "bad/player:1"}) assert(!parse_marker(s).revision);
 load_scene=load;
 request({"player-b",1});update(nullptr,nullptr);assert(load_calls==3); // Another account may have a lower revision.
 assert(started_revisions["player-b"]==1);
 assert(!admin_resync_2313::install());
}
