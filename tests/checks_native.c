#include "checks.h"

int rt_native_change_accessor(CPU *c, uint32_t entry);
void rt_native_destroy(void);

// GetChangeAccessor et NUM_FOR_EDICT d'engine.so, recopiés dans un bloc :
// code en +0 et +32, sv.edicts en +48, sv.edictchangeinfo en +52.
static uint32_t change_accessor_code(uint32_t block) {
    static const uint8_t accessor[] = {0x83, 0xec, 0x18, 0xff, 0x74, 0x24, 0x20, 0xe8, 0, 0, 0, 0,
                                       0x8b, 0x15, 0, 0, 0, 0, 0x83, 0xc4, 0x1c, 0x8d, 0x04, 0x82, 0xc3};
    static const uint8_t number[] = {0x8b, 0x44, 0x24, 0x04, 0x2b, 0x05, 0, 0, 0, 0, 0xc1, 0xf8, 0x04, 0xc3};
    memcpy(GPTR(block), accessor, sizeof(accessor));
    memcpy(GPTR(block + 32), number, sizeof(number));
    W32(block + 8, 32 - 12);        // call NUM_FOR_EDICT
    W32(block + 14, block + 52);    // &sv.edictchangeinfo
    W32(block + 32 + 6, block + 48);   // &sv.edicts
    rt_native_destroy();           // le remplacement garde les octets vérifiés par entrée
    return block;
}

static uint32_t call_accessor(uint32_t entry, uint32_t edict, int *handled) {
    CPU c = {0};
    uint32_t stack = rt_alloc(64);
    c.esp = stack + 48;
    W32(c.esp + 4, 0x131d49a8u);   // this (CVEngineServer)
    W32(c.esp + 8, edict);
    W32(c.esp, 0x12345678u);       // adresse de retour
    *handled = rt_native_change_accessor(&c, entry);
    assert(!*handled || (c.esp == stack + 52 && c.eip == 0x12345678u));
    rt_free(stack);
    return c.eax;
}

RT_CHECK(change_accessor_matches_engine_while_edicts_exist) {
    uint32_t block = rt_alloc(64), entry = change_accessor_code(block);
    int handled;
    W32(block + 48, 0x85a80000u);  // sv.edicts dans le tas invité (> 2 Go)
    W32(block + 52, 0x85b00000u);  // sv.edictchangeinfo
    assert(call_accessor(entry, 0x85a80000u, &handled) == 0x85b00000u && handled);
    assert(call_accessor(entry, 0x85a80000u + 16 * 37, &handled) == 0x85b00000u + 4 * 37 && handled);
    rt_free(block);
}

// Régression : quitter en pleine partie appelait GetChangeAccessor avec
// sv.edicts à zéro ; l'original rendait 0x6712c424, hors de toute projection.
RT_CHECK(change_accessor_without_edicts_returns_usable_scratch) {
    uint32_t block = rt_alloc(64), entry = change_accessor_code(block);
    int handled;
    W32(block + 48, 0);
    W32(block + 52, 0x85a8b810u);
    uint32_t accessor = call_accessor(entry, 0x85a83690u, &handled);
    assert(handled && accessor != 0x6712c424u && rt_guest_readable(accessor) && rt_guest_readable(accessor + 3));
    W16(accessor + 2, 0x1234);
    assert(call_accessor(entry, 0x85a83820u, &handled) == accessor && R16(accessor + 2) == 0);
    rt_free(block);
}

// Octets de NUM_FOR_EDICT inattendus : la fonction traduite reprend la main.
RT_CHECK(change_accessor_declines_unknown_code) {
    uint32_t block = rt_alloc(64), entry = change_accessor_code(block);
    int handled;
    W8(block + 32 + 10, 0x90);
    rt_native_destroy();
    call_accessor(entry, 0x85a83690u, &handled);
    assert(!handled);
    rt_free(block);
}

#include "posix.h"
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

int rt_native_open_for_write(CPU *c, uint32_t entry);
static char expected_write_path[PATH_MAX];
static int write_probe_calls;
static int write_probe(CPU *c, const char *name) {
    if (strcmp(name, "__native_write_probe")) return 0;
    assert(R32(c->esp + 4) == 0x11223344);
    assert(!strcmp(GPTR(R32(c->esp + 8)), expected_write_path));
    assert(!strcmp(GPTR(R32(c->esp + 12)), "wb"));
    assert(R32(c->esp + 16) == 0);
    ++write_probe_calls; c->eax = 0x1234;
    return 1;
}
RT_SHIM_PROVIDER(write_probe)

RT_CHECK(puzzlemaker_writepath_canonicalizes_root_before_original_checks) {
    char root[] = "/tmp/portal2-writepath-XXXXXX"; assert(mkdtemp(root));
    char game[PATH_MAX], linux_root[PATH_MAX], user[PATH_MAX];
    snprintf(game, sizeof(game), "%s/GameRoot", root); assert(!mkdir(game, 0755));
    snprintf(linux_root, sizeof(linux_root), "%s/Linux", root); assert(!mkdir(linux_root, 0755));
    snprintf(user, sizeof(user), "%s/User", root); assert(!rt_paths_init(game, linux_root, user));
    snprintf(expected_write_path, sizeof(expected_write_path), "%s/sdk_content/maps/preview.log", rt_game_root());
    char lower[PATH_MAX], canonical[PATH_MAX]; strcpy(lower, expected_write_path);
    for (char *p = lower; *p; ++p) *p = (char)tolower((unsigned char)*p);
    if (pathconf(game, _PC_CASE_SENSITIVE) == 0) {
        assert(rt_canonical_game_path(lower, canonical));
        assert(!strcmp(canonical, expected_write_path));
        uint32_t path = rt_alloc((uint32_t)strlen(lower) + 1), mode = rt_alloc(3);
        strcpy(GPTR(path), lower); strcpy(GPTR(mode), "wb");
        CPU c; rt_cpu_reset(&c); c.esp = 0x2001e000;
        W32(c.esp, 0x12345678); W32(c.esp + 4, 0x11223344); W32(c.esp + 8, path);
        W32(c.esp + 12, mode); W32(c.esp + 16, 0);
        write_probe_calls = 0;
        assert(rt_native_open_for_write(&c, rt_dynamic_import("__native_write_probe")));
        assert(c.eax == 0x1234 && c.eip == 0x12345678 && c.esp == 0x2001e004);
        assert(write_probe_calls == 1 && !strcmp(GPTR(path), lower));
        rt_free(path); rt_free(mode);
    } else assert(!rt_canonical_game_path(lower, canonical));
    assert(!rt_canonical_game_path("/etc/passwd", canonical));
    assert(!rmdir(user)); assert(!rmdir(linux_root)); assert(!rmdir(game)); assert(!rmdir(root));
}
