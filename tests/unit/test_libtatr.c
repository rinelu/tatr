#include "global.h"

#define LOG_IMPLEMENTATION
#include "log.h"

char g_repo_root[PATH_MAX] = {0};

#include "error.h"
#include "backend.h"
#include "issue.h"
#include "config.h"
#include "module.h"
#include "tatr.h"
#include "render.h"
#include "temp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
    char *id;
    char *data;
    size_t size;
} Mem_Record;

typedef struct {
    Mem_Record *items;
    size_t count, capacity;
} Mem_Store;

static Mem_Store g_mem = {0};

static Mem_Record *mem_find(const char *id)
{
    for (size_t i = 0; i < g_mem.count; i++)
        if (strcmp(g_mem.items[i].id, id) == 0)
            return &g_mem.items[i];
    return NULL;
}

static void mem_reset(void)
{
    for (size_t i = 0; i < g_mem.count; i++) {
        free(g_mem.items[i].id);
        free(g_mem.items[i].data);
    }
    free(g_mem.items);
    g_mem = (Mem_Store){0};
}

static Tatr_Error mem_load(Storage_Backend *self, const char *id, String_Builder *out)
{
    (void)self;
    Mem_Record *r = mem_find(id);
    if (!r) return TATR_ERR_NOT_FOUND;
    sb_append_buf(out, r->data, r->size);
    return TATR_OK;
}

static Tatr_Error mem_save(Storage_Backend *self, const char *id, const char *data, size_t size)
{
    (void)self;
    Mem_Record *r = mem_find(id);
    if (!r) return TATR_ERR_NOT_FOUND;
    free(r->data);
    r->data = malloc(size ? size : 1);
    memcpy(r->data, data, size);
    r->size = size;
    return TATR_OK;
}

static Tatr_Error mem_list(Storage_Backend *self, File_Paths *out)
{
    (void)self;
    for (size_t i = 0; i < g_mem.count; i++)
        da_append(out, g_mem.items[i].id);
    return TATR_OK;
}

static Tatr_Error mem_remove(Storage_Backend *self, const char *id)
{
    (void)self;
    for (size_t i = 0; i < g_mem.count; i++) {
        if (strcmp(g_mem.items[i].id, id) == 0) {
            free(g_mem.items[i].id);
            free(g_mem.items[i].data);
            g_mem.items[i] = g_mem.items[--g_mem.count];
            return TATR_OK;
        }
    }
    return TATR_ERR_NOT_FOUND;
}

static bool mem_exists(Storage_Backend *self, const char *id)
{
    (void)self;
    return mem_find(id) != NULL;
}

static Tatr_Error mem_create(Storage_Backend *self, const char *id)
{
    (void)self;
    if (mem_find(id)) return TATR_ERR_CONFLICT;
    Mem_Record r = { .id = strdup(id), .data = strdup(""), .size = 0 };
    da_append(&g_mem, r);
    return TATR_OK;
}

static const char *mem_attachment_dir(Storage_Backend *self, const char *id)
{
    (void)self; (void)id;
    return NULL;
}

static const Storage_Backend_Vtable MEM_VTABLE = {
    .load = mem_load, .save = mem_save, .list = mem_list, .remove = mem_remove,
    .exists = mem_exists, .create = mem_create, .attachment_dir = mem_attachment_dir,
};

static Storage_Backend g_mem_backend = { .vt = &MEM_VTABLE, .ctx = NULL };

// ------------------------------

static int g_failures = 0;
static int g_tests = 0;

#define CHECK(cond) do { \
    g_tests++; \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_STR(a, b) do { \
    g_tests++; \
    const char *_a = (a), *_b = (b); \
    if (strcmp(_a, _b) != 0) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a, _b); \
    } \
} while (0)

static void test_error_strings(void)
{
    CHECK_STR(tatr_strerror(TATR_OK), "ok");
    CHECK(strcmp(tatr_strerror(TATR_ERR_NOT_FOUND), tatr_strerror(TATR_ERR_IO)) != 0);
    CHECK(strlen(tatr_strerror((Tatr_Error)9999)) > 0); // never NULL/empty
}

static void test_issue_lifecycle_against_fake_backend(void)
{
    mem_reset();
    issue_set_backend(&g_mem_backend);

    CHECK(!issue_exists("abc"));

    Issue iss;
    CHECK(issue_create("abc", &iss));
    CHECK(issue_exists("abc"));

    iss.raw_sb = (String_Builder){0};
    sb_append_cstr(&iss.raw_sb, "title: hello\nstatus: open\npriority: high\ntags: x,y\ncreated: 2026-01-01\n---\nbody text\n");
    CHECK(issue_save(&iss));
    issue_free(&iss);

    Issue loaded;
    CHECK(issue_load("abc", &loaded));
    CHECK(sv_eq_cstr(loaded.title, "hello"));
    CHECK(loaded.status == ISSUE_SOPEN);
    CHECK(loaded.priority == ISSUE_PHIGH);
    CHECK(issue_has_tag(&loaded, "y"));
    issue_free(&loaded);

    File_Paths ids = {0};
    CHECK(issue_list_ids(&ids));
    CHECK(ids.count == 1);
    da_free(ids);

    CHECK(issue_delete("abc"));
    CHECK(!issue_exists("abc"));
    
    // restore default (flatfile) for later tests
    issue_set_backend(NULL);
    mem_reset();
}

static void test_tatr_issue_list_filters(void)
{
    mem_reset();
    issue_set_backend(&g_mem_backend);

    Issue a, b;
    issue_create("a", &a);
    a.raw_sb = (String_Builder){0};
    sb_append_cstr(&a.raw_sb, "title: Open one\nstatus: open\npriority: normal\ntags: \ncreated: 2026-01-01\n---\n");
    issue_save(&a);
    issue_free(&a);

    issue_create("b", &b);
    b.raw_sb = (String_Builder){0};
    sb_append_cstr(&b.raw_sb, "title: Closed one\nstatus: closed\npriority: normal\ntags: \ncreated: 2026-01-01\n---\n");
    issue_save(&b);
    issue_free(&b);

    Tatr_Issue_Filter f = { .status = ISSUE_SINVALID, .priority = ISSUE_PINVALID, .tag = NULL };
    Tatr_Issue_List_Result r;

    CHECK(tatr_issue_list(&f, true, &r) == TATR_OK);
    CHECK(r.issues.count == 1);
    if (r.issues.count == 1) CHECK(sv_eq_cstr(r.issues.items[0].id, "a"));

    f.include_closed = true;
    CHECK(tatr_issue_list(&f, true, &r) == TATR_OK);
    CHECK(r.issues.count == 2);

    issue_set_backend(NULL);
    mem_reset();
}

static char *slurp_tmpfile(FILE *f)
{
    long len = ftell(f);
    rewind(f);

    char *buf = malloc((size_t)len + 1);
    size_t n = fread(buf, 1, (size_t)len, f);
    buf[n] = '\0';

    fclose(f);
    return buf;
}

static void test_render_json_issue_list(void)
{
    Tatr_Issue_Summary items[1] = {{
        .id       = sv_from_cstr("42"),
        .title    = sv_from_cstr("Fix \"the\" bug"),
        .status   = ISSUE_SOPEN,
        .priority = ISSUE_PCRITICAL,
    }};
    Tatr_Issue_List_Result r = { .show_header = true, .issues = { .items = items, .count = 1 } };

    FILE *mem = tmpfile();
    renderer_for(TATR_FMT_JSON)->issue_list(mem, &r);
    char *buf = slurp_tmpfile(mem);

    CHECK_STR(buf,
        "[{\"id\":\"42\",\"title\":\"Fix \\\"the\\\" bug\",\"status\":\"open\",\"priority\":\"critical\"}]\n");
    free(buf);
}

static void test_render_json_error(void)
{
    FILE *mem = tmpfile();
    renderer_for(TATR_FMT_JSON)->error(mem, TATR_ERR_NOT_FOUND, "issue 'x'");
    char *buf = slurp_tmpfile(mem);

    CHECK_STR(buf, "{\"error\":\"not found\",\"context\":\"issue 'x'\"}\n");
    free(buf);
}

static void test_config_schema_validation(void)
{
    const Config_Key_Def *priority = config_key_def("default_priority");
    CHECK(priority != NULL);
    if (priority) {
        CHECK(priority->type == CFG_ENUM);
        CHECK(config_validate(priority, "high"));
        CHECK(!config_validate(priority, "nonsense"));
    }

    const Config_Key_Def *show_closed = config_key_def("list.show_closed");
    CHECK(show_closed != NULL);
    if (show_closed) {
        CHECK(show_closed->type == CFG_BOOL);
        CHECK(config_validate(show_closed, "true"));
        CHECK(config_validate(show_closed, "0"));
        CHECK(!config_validate(show_closed, "yes")); /* ambiguous, rejected */
    }

    const Config_Key_Def *limit = config_key_def("list.limit");
    CHECK(limit != NULL);
    if (limit) {
        CHECK(limit->type == CFG_INT);
        CHECK(config_validate(limit, "42"));
        CHECK(config_validate(limit, "-1"));
        CHECK(!config_validate(limit, "abc"));
    }

    CHECK(config_key_def("no.such.key") == NULL);
}

static void test_module_registry(void)
{
#ifdef TATR_MODULE_EXPORT
    const Tatr_Module *exp = tatr_module_find("export");
    CHECK(exp != NULL);
    if (exp) {
        CHECK_STR(exp->name, "export");
        CHECK((exp->capabilities & TATR_MODCAP_CLI_COMMAND) != 0);
    }
#else
    CHECK(tatr_module_find("export") == NULL);
#endif
    CHECK(tatr_module_find("no-such-module") == NULL);
    CHECK(tatr_modules_init() == TATR_OK);
    for (size_t i = 0; i < tatr_module_count; i++) {
        CHECK(tatr_modules[i]->name != NULL);
        CHECK(tatr_modules[i]->description != NULL);
    }
}

int main(void)
{
    temp_init();

    test_error_strings();
    test_issue_lifecycle_against_fake_backend();
    test_tatr_issue_list_filters();
    test_render_json_issue_list();
    test_render_json_error();
    test_config_schema_validation();
    test_module_registry();

    temp_destroy();

    printf("%d tests, %d failures\n", g_tests, g_failures);
    return g_failures ? 1 : 0;
}
