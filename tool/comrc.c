/*
 * vearc.c - VeaOS NT-Compatible Resource Compiler (.rc -> ELF32 .o / raw .rsrc)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* =========================================================================
 * 1. Struktur Biner PE .rsrc (Identik 1:1 dengan resource.h VeaOS)
 * ========================================================================= */
#define VEA_RSRC_NAME_IS_STRING    0x80000000U
#define VEA_RSRC_DATA_IS_DIRECTORY 0x80000000U

#define VEA_RT_CURSOR       1
#define VEA_RT_BITMAP       2
#define VEA_RT_ICON         3
#define VEA_RT_MENU         4
#define VEA_RT_DIALOG       5
#define VEA_RT_STRING       6
#define VEA_RT_FONTDIR      7
#define VEA_RT_FONT         8
#define VEA_RT_ACCELERATOR  9
#define VEA_RT_RCDATA       10
#define VEA_RT_MESSAGETABLE 11
#define VEA_RT_GROUP_CURSOR 12
#define VEA_RT_GROUP_ICON   14
#define VEA_RT_VERSION      16
#define VEA_RT_MANIFEST     24
#define VEA_RT_THEME        256

#pragma pack(push, 1)
typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint16_t NumberOfNamedEntries;
    uint16_t NumberOfIdEntries;
} VEA_RES_DIR;

typedef struct {
    uint32_t Name;
    uint32_t OffsetToData;
} VEA_RES_DIR_ENTRY;

typedef struct {
    uint32_t OffsetToData;
    uint32_t Size;
    uint32_t CodePage;
    uint32_t Reserved;
} VEA_RES_DATA_ENTRY;

/* Struktur Minimal ELF32 Relocatable Object (.o) */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
} Elf32_Sym;
#pragma pack(pop)

/* =========================================================================
 * 2. Tabel Macro (#define) & Include Paths
 * ========================================================================= */
typedef struct {
    char     name[128];
    uint32_t value;
} MacroDef;

static MacroDef *gMacros = NULL;
static size_t    gMacroCount = 0;
static size_t    gMacroCap = 0;

static const char *gIncDirs[32];
static size_t      gIncDirCount = 0;

static void add_macro(const char *name, uint32_t value) {
    for (size_t i = 0; i < gMacroCount; i++) {
        if (strcmp(gMacros[i].name, name) == 0) {
            gMacros[i].value = value;
            return;
        }
    }
    if (gMacroCount == gMacroCap) {
        gMacroCap = gMacroCap ? gMacroCap * 2 : 128;
        gMacros = realloc(gMacros, gMacroCap * sizeof(MacroDef));
    }
    snprintf(gMacros[gMacroCount].name, sizeof(gMacros[0].name), "%s", name);
    gMacros[gMacroCount].value = value;
    gMacroCount++;
}

static bool lookup_macro(const char *name, uint32_t *out) {
    for (size_t i = 0; i < gMacroCount; i++) {
        if (strcmp(gMacros[i].name, name) == 0) {
            *out = gMacros[i].value;
            return true;
        }
    }
    return false;
}

static void init_builtin_macros(void) {
    add_macro("SUBLANG_NEUTRAL", 0x00);
    add_macro("SUBLANG_DEFAULT", 0x01);
    add_macro("LANG_NEUTRAL",    0x00);
    add_macro("LANG_ENGLISH",    0x09);
    add_macro("LANG_JAPANESE",   0x11);
    add_macro("LANG_INDONESIAN", 0x21);

    add_macro("RT_CURSOR",       VEA_RT_CURSOR);
    add_macro("RT_BITMAP",       VEA_RT_BITMAP);
    add_macro("RT_ICON",         VEA_RT_ICON);
    add_macro("RT_MENU",         VEA_RT_MENU);
    add_macro("RT_DIALOG",       VEA_RT_DIALOG);
    add_macro("RT_STRING",       VEA_RT_STRING);
    add_macro("RT_FONTDIR",      VEA_RT_FONTDIR);
    add_macro("RT_FONT",         VEA_RT_FONT);
    add_macro("RT_ACCELERATOR",  VEA_RT_ACCELERATOR);
    add_macro("RT_RCDATA",       VEA_RT_RCDATA);
    add_macro("RT_MESSAGETABLE", VEA_RT_MESSAGETABLE);
    add_macro("RT_GROUP_CURSOR", VEA_RT_GROUP_CURSOR);
    add_macro("RT_GROUP_ICON",   VEA_RT_GROUP_ICON);
    add_macro("RT_VERSION",      VEA_RT_VERSION);
    add_macro("RT_MANIFEST",     VEA_RT_MANIFEST);
    add_macro("RT_THEME",        VEA_RT_THEME);
}

/* =========================================================================
 * 3. Koleksi Item Resource & StringTable
 * ========================================================================= */
typedef struct {
    bool     type_is_str;
    uint16_t type_id;
    char     type_str[128];

    bool     name_is_str;
    uint16_t name_id;
    char     name_str[128];

    uint16_t lang_id;
    uint32_t codepage;
    uint8_t *data;
    uint32_t size;
} RcItem;

typedef struct {
    uint16_t lang_id;
    uint16_t string_id;
    char    *utf8_text;
} RcStringEntry;

static RcItem        *gItems = NULL;
static size_t         gItemCount = 0;
static size_t         gItemCap = 0;

static RcStringEntry *gStrings = NULL;
static size_t         gStringCount = 0;
static size_t         gStringCap = 0;

static void add_rc_item(RcItem item) {
    if (gItemCount == gItemCap) {
        gItemCap = gItemCap ? gItemCap * 2 : 64;
        gItems = realloc(gItems, gItemCap * sizeof(RcItem));
    }
    gItems[gItemCount++] = item;
}

static void add_rc_string(uint16_t lang_id, uint16_t str_id, char *text) {
    if (gStringCount == gStringCap) {
        gStringCap = gStringCap ? gStringCap * 2 : 64;
        gStrings = realloc(gStrings, gStringCap * sizeof(RcStringEntry));
    }
    gStrings[gStringCount++] = (RcStringEntry){ lang_id, str_id, text };
}

/* =========================================================================
 * 4. Helper Path, File I/O, & Expression Evaluator
 * ========================================================================= */
static void get_dir_part(const char *path, char *out, size_t out_sz) {
    const char *slash1 = strrchr(path, '/');
    const char *slash2 = strrchr(path, '\\');
    const char *last = (slash1 > slash2) ? slash1 : slash2;
    if (!last) {
        snprintf(out, out_sz, ".");
    } else {
        size_t len = (size_t)(last - path);
        if (len >= out_sz) len = out_sz - 1;
        memcpy(out, path, len);
        out[len] = '\0';
    }
}

static bool resolve_file(const char *base_dir, const char *rel, char *out, size_t out_sz) {
    FILE *f = fopen(rel, "rb");
    if (f) { fclose(f); snprintf(out, out_sz, "%s", rel); return true; }

    snprintf(out, out_sz, "%s/%s", base_dir, rel);
    f = fopen(out, "rb");
    if (f) { fclose(f); return true; }

    for (size_t i = 0; i < gIncDirCount; i++) {
        snprintf(out, out_sz, "%s/%s", gIncDirs[i], rel);
        f = fopen(out, "rb");
        if (f) { fclose(f); return true; }
    }
    return false;
}

static uint8_t *read_entire_file(const char *path, uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = 0;
    if (out_size) *out_size = (uint32_t)n;
    return buf;
}

static void str_to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* Evaluasi ekspresi integer sederhana untuk #define dan ID (misal: 100 + 1, 0x0400 | 0x21) */
static uint32_t eval_int_expr(const char **p) {
    uint32_t acc = 0;
    char op = '+';
    while (**p) {
        while (**p == ' ' || **p == '\t') (*p)++;
        if (**p == '(') {
            (*p)++;
            uint32_t v = eval_int_expr(p);
            if (**p == ')') (*p)++;
            if (op == '+') acc += v;
            else if (op == '-') acc -= v;
            else if (op == '|') acc |= v;
        } else if (isdigit((unsigned char)**p)) {
            char *end = NULL;
            uint32_t v = (uint32_t)strtoul(*p, &end, 0);
            *p = end;
            if (op == '+') acc += v;
            else if (op == '-') acc -= v;
            else if (op == '|') acc |= v;
        } else if (isalpha((unsigned char)**p) || **p == '_') {
            char ident[128];
            size_t k = 0;
            while ((isalnum((unsigned char)**p) || **p == '_') && k + 1 < sizeof(ident)) {
                ident[k++] = *(*p)++;
            }
            ident[k] = '\0';
            uint32_t v = 0;
            lookup_macro(ident, &v);
            if (op == '+') acc += v;
            else if (op == '-') acc -= v;
            else if (op == '|') acc |= v;
        } else {
            break;
        }

        while (**p == ' ' || **p == '\t') (*p)++;
        if (**p == '+' || **p == '-' || **p == '|') {
            op = *(*p)++;
        } else {
            break;
        }
    }
    return acc;
}

static char (*gParsedHeaders)[512] = NULL;
static size_t gParsedHeaderCount = 0;
static size_t gParsedHeaderCap   = 0;

static void normalize_path(const char *in, char *out, size_t out_sz) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", in);
    for (char *s = tmp; *s; s++) {
        if (*s == '\\') *s = '/';
    }

    const char *parts[64];
    size_t count = 0;
    bool is_abs = (tmp[0] == '/');

    char *p = tmp;
    while (*p) {
        while (*p == '/') *p++ = '\0';
        if (!*p) break;
        char *seg = p;
        while (*p && *p != '/') p++;
        if (strcmp(seg, ".") == 0) {
            continue;
        } else if (strcmp(seg, "..") == 0 && count > 0 && strcmp(parts[count - 1], "..") != 0) {
            count--;
        } else if (count < 64) {
            parts[count++] = seg;
        }
    }

    size_t pos = 0;
    if (is_abs && pos + 1 < out_sz) out[pos++] = '/';
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(parts[i]);
        if (i > 0 && pos + 1 < out_sz) out[pos++] = '/';
        if (pos + len < out_sz) {
            memcpy(out + pos, parts[i], len);
            pos += len;
        }
    }
    out[pos] = '\0';
}

static bool mark_header_parsed(const char *path) {
    char norm[512];
    normalize_path(path, norm, sizeof(norm));
    for (size_t i = 0; i < gParsedHeaderCount; i++) {
        if (strcmp(gParsedHeaders[i], norm) == 0) {
            return false;
        }
    }
    if (gParsedHeaderCount == gParsedHeaderCap) {
        gParsedHeaderCap = gParsedHeaderCap ? gParsedHeaderCap * 2 : 64;
        gParsedHeaders = realloc(gParsedHeaders, gParsedHeaderCap * sizeof(*gParsedHeaders));
    }
    snprintf(gParsedHeaders[gParsedHeaderCount++], sizeof(*gParsedHeaders), "%s", norm);
    return true;
}

/* =========================================================================
 * 5. Parser Header (#define) & Script .rc
 * ========================================================================= */
static void parse_header_defines(const char *path) {
    if (!mark_header_parsed(path)) return;
    
    uint32_t sz = 0;
    char *src = (char *)read_entire_file(path, &sz);
    if (!src) return;

    char dir[512];
    get_dir_part(path, dir, sizeof(dir));

    char *cursor = src;
    while (*cursor) {
        char *line = cursor;
        while (*cursor && *cursor != '\r' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor) {
            *cursor++ = '\0';
            while (*cursor == '\r' || *cursor == '\n') {
                cursor++;
            }
        }

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "#define", 7) == 0 && isspace((unsigned char)p[7])) {
            p += 7;
            while (*p == ' ' || *p == '\t') p++;
            char name[128];
            size_t k = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && k + 1 < sizeof(name)) {
                name[k++] = *p++;
            }
            name[k] = '\0';
            if (*p != '(' && k > 0) {
                while (*p == ' ' || *p == '\t') p++;
                if (isdigit((unsigned char)*p) || *p == '(' || isalpha((unsigned char)*p)) {
                    const char *ep = p;
                    uint32_t val = eval_int_expr(&ep);
                    add_macro(name, val);
                }
            }
        } else if (strncmp(p, "#include", 8) == 0) {
            p += 8;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '"' || *p == '<') {
                char endc = (*p == '"') ? '"' : '>';
                p++;
                char inc_name[256], resolved[512];
                size_t k = 0;
                while (*p && *p != endc && k + 1 < sizeof(inc_name)) inc_name[k++] = *p++;
                inc_name[k] = '\0';
                if (resolve_file(dir, inc_name, resolved, sizeof(resolved))) {
                    parse_header_defines(resolved);
                }
            }
        }
    }
    free(src);
}

typedef struct {
    const char *src;
    size_t      pos;
    size_t      len;
    int         line;
    char        base_dir[512];
} Lexer;

static void lex_skip_ws_and_comments(Lexer *L, bool stop_at_newline) {
    while (L->pos < L->len) {
        char c = L->src[L->pos];
        if (c == '\n') {
            if (stop_at_newline) return;
            L->line++;
            L->pos++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == ',') {
            L->pos++;
        } else if (c == '/' && L->pos + 1 < L->len && L->src[L->pos + 1] == '/') {
            while (L->pos < L->len && L->src[L->pos] != '\n') L->pos++;
        } else if (c == '/' && L->pos + 1 < L->len && L->src[L->pos + 1] == '*') {
            L->pos += 2;
            while (L->pos + 1 < L->len && !(L->src[L->pos] == '*' && L->src[L->pos + 1] == '/')) {
                if (L->src[L->pos] == '\n') L->line++;
                L->pos++;
            }
            if (L->pos + 1 < L->len) L->pos += 2;
        } else {
            break;
        }
    }
}

static bool lex_next_token(Lexer *L, char *out, size_t out_sz, bool *is_quoted) {
    *is_quoted = false;
    lex_skip_ws_and_comments(L, false);
    if (L->pos >= L->len) return false;

    char c = L->src[L->pos];
    if (c == '"') {
        *is_quoted = true;
        L->pos++;
        size_t k = 0;
        while (L->pos < L->len) {
            char ch = L->src[L->pos++];
            if (ch == '"') {
                /* "" di .rc berarti literal " */
                if (L->pos < L->len && L->src[L->pos] == '"') {
                    L->pos++;
                    if (k + 1 < out_sz) out[k++] = '"';
                    continue;
                }
                break;
            }
            if (ch == '\\' && L->pos < L->len) {
                char esc = L->src[L->pos++];
                if (esc == 'n') ch = '\n';
                else if (esc == 'r') ch = '\r';
                else if (esc == 't') ch = '\t';
                else if (esc == '0') ch = '\0';
                else ch = esc;
            }
            if (k + 1 < out_sz) out[k++] = ch;
        }
        out[k] = '\0';
        return true;
    }

    if (c == '{' || c == '}') {
        out[0] = c;
        out[1] = '\0';
        L->pos++;
        return true;
    }

    size_t k = 0;
    while (L->pos < L->len) {
        char ch = L->src[L->pos];
        if (isspace((unsigned char)ch) || ch == ',' || ch == '"' || ch == '{' || ch == '}') break;
        if (ch == '/' && L->pos + 1 < L->len && (L->src[L->pos + 1] == '/' || L->src[L->pos + 1] == '*')) break;
        if (k + 1 < out_sz) out[k++] = ch;
        L->pos++;
    }
    out[k] = '\0';
    return k > 0;
}

static bool parse_id_or_string(const char *tok, bool quoted, bool *out_is_str, uint16_t *out_id, char *out_str, size_t str_sz) {
    if (quoted) {
        *out_is_str = true;
        snprintf(out_str, str_sz, "%s", tok);
        str_to_upper(out_str);
        return true;
    }
    if (isdigit((unsigned char)tok[0])) {
        *out_is_str = false;
        *out_id = (uint16_t)strtoul(tok, NULL, 0);
        return true;
    }
    uint32_t val = 0;
    if (lookup_macro(tok, &val)) {
        *out_is_str = false;
        *out_id = (uint16_t)val;
        return true;
    }
    /* Nama string tidak dikutip (standar NT .rc untuk nama resource) */
    *out_is_str = true;
    snprintf(out_str, str_sz, "%s", tok);
    str_to_upper(out_str);
    return true;
}

static bool is_memory_flag(const char *tok) {
    return strcasecmp(tok, "DISCARDABLE") == 0 ||
           strcasecmp(tok, "PRELOAD") == 0 ||
           strcasecmp(tok, "LOADONCALL") == 0 ||
           strcasecmp(tok, "MOVEABLE") == 0 ||
           strcasecmp(tok, "FIXED") == 0 ||
           strcasecmp(tok, "PURE") == 0 ||
           strcasecmp(tok, "IMPURE") == 0;
}

static bool map_resource_type(const char *tok, bool quoted, bool *out_is_str, uint16_t *out_id, char *out_str, size_t str_sz) {
    if (!quoted) {
        if (strcasecmp(tok, "CURSOR") == 0)       { *out_is_str = false; *out_id = VEA_RT_CURSOR; return true; }
        if (strcasecmp(tok, "BITMAP") == 0)       { *out_is_str = false; *out_id = VEA_RT_BITMAP; return true; }
        if (strcasecmp(tok, "ICON") == 0)         { *out_is_str = false; *out_id = VEA_RT_ICON; return true; }
        if (strcasecmp(tok, "MENU") == 0)         { *out_is_str = false; *out_id = VEA_RT_MENU; return true; }
        if (strcasecmp(tok, "DIALOG") == 0)       { *out_is_str = false; *out_id = VEA_RT_DIALOG; return true; }
        if (strcasecmp(tok, "FONT") == 0)         { *out_is_str = false; *out_id = VEA_RT_FONT; return true; }
        if (strcasecmp(tok, "RCDATA") == 0)       { *out_is_str = false; *out_id = VEA_RT_RCDATA; return true; }
        if (strcasecmp(tok, "MESSAGETABLE") == 0) { *out_is_str = false; *out_id = VEA_RT_MESSAGETABLE; return true; }
        if (strcasecmp(tok, "VERSIONINFO") == 0)  { *out_is_str = false; *out_id = VEA_RT_VERSION; return true; }
        if (strcasecmp(tok, "MANIFEST") == 0)     { *out_is_str = false; *out_id = VEA_RT_MANIFEST; return true; }
        if (strcasecmp(tok, "THEME") == 0)        { *out_is_str = false; *out_id = VEA_RT_THEME; return true; }
    }
    return parse_id_or_string(tok, quoted, out_is_str, out_id, out_str, str_sz);
}

static bool parse_rc_script(const char *rc_path) {
    uint32_t src_len = 0;
    char *src = (char *)read_entire_file(rc_path, &src_len);
    if (!src) {
        fprintf(stderr, "vearc: gagal membuka '%s'\n", rc_path);
        return false;
    }

    Lexer L = { .src = src, .pos = 0, .len = src_len, .line = 1 };
    get_dir_part(rc_path, L.base_dir, sizeof(L.base_dir));

    uint16_t current_lang = 0x0000; /* VEA_LANG_NEUTRAL */
    char tok[512];
    bool quoted = false;

    while (lex_next_token(&L, tok, sizeof(tok), &quoted)) {
        /* Tangani #include dan #define di dalam .rc */
        if (!quoted && tok[0] == '#') {
            char dir_kw[64];
            if (strcmp(tok, "#") == 0) {
                if (!lex_next_token(&L, dir_kw, sizeof(dir_kw), &quoted)) break;
            } else {
                snprintf(dir_kw, sizeof(dir_kw), "%s", tok + 1);
            }

            if (strcmp(dir_kw, "include") == 0) {
                char inc_file[256], resolved[512];
                if (lex_next_token(&L, inc_file, sizeof(inc_file), &quoted)) {
                    if (inc_file[0] == '<') {
                        size_t ilen = strlen(inc_file);
                        if (ilen > 2 && inc_file[ilen - 1] == '>') {
                            inc_file[ilen - 1] = '\0';
                            memmove(inc_file, inc_file + 1, ilen - 1);
                        }
                    }
                    if (resolve_file(L.base_dir, inc_file, resolved, sizeof(resolved))) {
                        parse_header_defines(resolved);
                    } else {
                        fprintf(stderr, "vearc(%d): warning header '%s' tidak ditemukan\n", L.line, inc_file);
                    }
                }
            } else if (strcmp(dir_kw, "define") == 0) {
                char mname[128], mval[128];
                if (lex_next_token(&L, mname, sizeof(mname), &quoted) &&
                    lex_next_token(&L, mval, sizeof(mval), &quoted)) {
                    const char *ep = mval;
                    add_macro(mname, eval_int_expr(&ep));
                }
            } else {
                /* Lewati direktif preprocessor lain (#pragma, dll) sampai akhir baris */
                while (L.pos < L.len && L.src[L.pos] != '\n') L.pos++;
            }
            continue;
        }

        /* LANGUAGE <FullLangId>  ATAU  LANGUAGE <Primary>, <Sublang> */
        if (!quoted && strcasecmp(tok, "LANGUAGE") == 0) {
            char p_tok[128];
            if (!lex_next_token(&L, p_tok, sizeof(p_tok), &quoted)) {
                fprintf(stderr, "comrc(%d): sintaks LANGUAGE tidak lengkap\n", L.line);
                free(src);
                return false;
            }
            uint32_t prim = 0;
            if (!lookup_macro(p_tok, &prim)) { const char *ep = p_tok; prim = eval_int_expr(&ep); }

            /* Cek apakah ada argumen kedua (Sublanguage) di baris yang sama */
            lex_skip_ws_and_comments(&L, true);
            if (L.pos < L.len && L.src[L.pos] != '\n') {
                char s_tok[128];
                if (lex_next_token(&L, s_tok, sizeof(s_tok), &quoted)) {
                    uint32_t sub = 0;
                    if (!lookup_macro(s_tok, &sub)) { const char *ep = s_tok; sub = eval_int_expr(&ep); }
                    current_lang = (uint16_t)(((sub & 0x3F) << 10) | (prim & 0x3FF));
                    continue;
                }
            }

            /* 1 argumen penuh (misal: LANGUAGE LANG_INDONESIAN -> 0x0421) */
            current_lang = (uint16_t)(prim & 0xFFFF);
            continue;
        }

        /* STRINGTABLE [flags] BEGIN ... END */
        if (!quoted && strcasecmp(tok, "STRINGTABLE") == 0) {
            char st_tok[512];
            while (lex_next_token(&L, st_tok, sizeof(st_tok), &quoted)) {
                if (!quoted && (strcasecmp(st_tok, "BEGIN") == 0 || strcmp(st_tok, "{") == 0)) break;
            }
            while (lex_next_token(&L, st_tok, sizeof(st_tok), &quoted)) {
                if (!quoted && (strcasecmp(st_tok, "END") == 0 || strcmp(st_tok, "}") == 0)) break;
                uint32_t str_id = 0;
                if (!lookup_macro(st_tok, &str_id)) {
                    const char *ep = st_tok;
                    str_id = eval_int_expr(&ep);
                }
                char text_buf[2048];
                if (!lex_next_token(&L, text_buf, sizeof(text_buf), &quoted) || !quoted) {
                    fprintf(stderr, "vearc(%d): mengharapkan string literal di STRINGTABLE\n", L.line);
                    free(src);
                    return false;
                }
                add_rc_string(current_lang, (uint16_t)str_id, strdup(text_buf));
            }
            continue;
        }

        /* Pernyataan Resource standar: <NameID> <Type> [Flags...] "<FilePath>" */
        RcItem item;
        memset(&item, 0, sizeof(item));
        item.lang_id  = current_lang;
        item.codepage = 65001;

        parse_id_or_string(tok, quoted, &item.name_is_str, &item.name_id, item.name_str, sizeof(item.name_str));

        char type_tok[256];
        if (!lex_next_token(&L, type_tok, sizeof(type_tok), &quoted)) {
            fprintf(stderr, "vearc(%d): tipe resource hilang setelah '%s'\n", L.line, tok);
            free(src);
            return false;
        }
        map_resource_type(type_tok, quoted, &item.type_is_str, &item.type_id, item.type_str, sizeof(item.type_str));

        char file_tok[512];
        while (true) {
            if (!lex_next_token(&L, file_tok, sizeof(file_tok), &quoted)) {
                fprintf(stderr, "vearc(%d): path file resource hilang\n", L.line);
                free(src);
                return false;
            }
            if (!quoted && is_memory_flag(file_tok)) continue;
            break;
        }

        char resolved_asset[512];
        if (!resolve_file(L.base_dir, file_tok, resolved_asset, sizeof(resolved_asset))) {
            fprintf(stderr, "vearc(%d): file aset '%s' tidak ditemukan\n", L.line, file_tok);
            free(src);
            return false;
        }

        item.data = read_entire_file(resolved_asset, &item.size);
        if (!item.data) {
            fprintf(stderr, "vearc(%d): gagal membaca '%s'\n", L.line, resolved_asset);
            free(src);
            return false;
        }
        add_rc_item(item);
    }

    free(src);
    return true;
}

/* =========================================================================
 * 6. Konversi STRINGTABLE menjadi Blok RT_STRING (16 string / blok)
 * ========================================================================= */
static int cmp_rc_string(const void *a, const void *b) {
    const RcStringEntry *sa = (const RcStringEntry *)a;
    const RcStringEntry *sb = (const RcStringEntry *)b;
    if (sa->lang_id != sb->lang_id) return (int)sa->lang_id - (int)sb->lang_id;
    return (int)sa->string_id - (int)sb->string_id;
}

static void flush_string_tables(void) {
    if (gStringCount == 0) return;
    qsort(gStrings, gStringCount, sizeof(RcStringEntry), cmp_rc_string);

    size_t i = 0;
    while (i < gStringCount) {
        uint16_t lang  = gStrings[i].lang_id;
        uint16_t block = (uint16_t)((gStrings[i].string_id >> 4) + 1);

        const char *slots[16] = { 0 };
        while (i < gStringCount &&
               gStrings[i].lang_id == lang &&
               ((gStrings[i].string_id >> 4) + 1) == block) {
            slots[gStrings[i].string_id & 0x0F] = gStrings[i].utf8_text;
            i++;
        }

        /* Hitung total byte UTF-16LE untuk blok 16-entry */
        uint32_t total_bytes = 0;
        for (int s = 0; s < 16; s++) {
            uint16_t ch_len = slots[s] ? (uint16_t)strlen(slots[s]) : 0;
            total_bytes += 2 + (uint32_t)ch_len * 2;
        }

        uint8_t *blob = calloc(1, total_bytes);
        uint32_t off = 0;
        for (int s = 0; s < 16; s++) {
            uint16_t ch_len = slots[s] ? (uint16_t)strlen(slots[s]) : 0;
            memcpy(blob + off, &ch_len, 2);
            off += 2;
            for (uint16_t c = 0; c < ch_len; c++) {
                uint16_t u16 = (uint8_t)slots[s][c];
                memcpy(blob + off, &u16, 2);
                off += 2;
            }
        }

        RcItem item;
        memset(&item, 0, sizeof(item));
        item.type_is_str = false;
        item.type_id     = VEA_RT_STRING;
        item.name_is_str = false;
        item.name_id     = block;
        item.lang_id     = lang;
        item.codepage    = 1200; /* UTF-16LE */
        item.data        = blob;
        item.size        = total_bytes;
        add_rc_item(item);
    }
}

/* =========================================================================
 * 7. Serializer Pohon 3-Tingkat PE .rsrc
 * ========================================================================= */
static int cmp_rc_item(const void *a, const void *b) {
    const RcItem *ia = (const RcItem *)a;
    const RcItem *ib = (const RcItem *)b;

    /* 1. Urutkan Type: Named alfabetis dulu, baru ID numerik */
    if (ia->type_is_str != ib->type_is_str) return ia->type_is_str ? -1 : 1;
    if (ia->type_is_str) {
        int c = strcmp(ia->type_str, ib->type_str);
        if (c != 0) return c;
    } else if (ia->type_id != ib->type_id) {
        return (int)ia->type_id - (int)ib->type_id;
    }

    /* 2. Urutkan Name: Named alfabetis dulu, baru ID numerik */
    if (ia->name_is_str != ib->name_is_str) return ia->name_is_str ? -1 : 1;
    if (ia->name_is_str) {
        int c = strcmp(ia->name_str, ib->name_str);
        if (c != 0) return c;
    } else if (ia->name_id != ib->name_id) {
        return (int)ia->name_id - (int)ib->name_id;
    }

    /* 3. Urutkan LANGID menaik */
    return (int)ia->lang_id - (int)ib->lang_id;
}

static bool same_type(const RcItem *a, const RcItem *b) {
    if (a->type_is_str != b->type_is_str) return false;
    return a->type_is_str ? (strcmp(a->type_str, b->type_str) == 0) : (a->type_id == b->type_id);
}

static bool same_name(const RcItem *a, const RcItem *b) {
    if (a->name_is_str != b->name_is_str) return false;
    return a->name_is_str ? (strcmp(a->name_str, b->name_str) == 0) : (a->name_id == b->name_id);
}

static uint8_t *build_rsrc_binary(uint32_t *out_total_size) {
    flush_string_tables();
    if (gItemCount == 0) {
        fprintf(stderr, "vearc: tidak ada resource untuk di-compile\n");
        return NULL;
    }

    qsort(gItems, gItemCount, sizeof(RcItem), cmp_rc_item);

    /* Validasi duplikat (Type, Name, Lang) */
    for (size_t i = 1; i < gItemCount; i++) {
        if (same_type(&gItems[i - 1], &gItems[i]) &&
            same_name(&gItems[i - 1], &gItems[i]) &&
            gItems[i - 1].lang_id == gItems[i].lang_id) {
            fprintf(stderr, "vearc: error duplikat resource dengan LANGID 0x%04X\n", gItems[i].lang_id);
            return NULL;
        }
    }

    /* Hitung jumlah direktori di setiap level */
    size_t type_dir_count = 0;
    size_t name_dir_count = 0;
    for (size_t i = 0; i < gItemCount; ) {
        type_dir_count++;
        size_t j = i;
        while (j < gItemCount && same_type(&gItems[i], &gItems[j])) {
            name_dir_count++;
            size_t k = j;
            while (k < gItemCount && same_type(&gItems[j], &gItems[k]) && same_name(&gItems[j], &gItems[k])) {
                k++;
            }
            j = k;
        }
        i = j;
    }

    /* Hitung total ukuran string UTF-16LE untuk semua named entries */
    uint32_t string_pool_bytes = 0;
    for (size_t i = 0; i < gItemCount; ) {
        if (gItems[i].type_is_str) {
            string_pool_bytes += 2 + (uint32_t)strlen(gItems[i].type_str) * 2;
        }
        size_t j = i;
        while (j < gItemCount && same_type(&gItems[i], &gItems[j])) {
            if (gItems[j].name_is_str) {
                string_pool_bytes += 2 + (uint32_t)strlen(gItems[j].name_str) * 2;
            }
            size_t k = j;
            while (k < gItemCount && same_type(&gItems[j], &gItems[k]) && same_name(&gItems[j], &gItems[k])) {
                k++;
            }
            j = k;
        }
        i = j;
    }
    string_pool_bytes = (string_pool_bytes + 3U) & ~3U;

    /* Offset tiap zona di dalam section .rsrc */
    uint32_t off_root       = 0;
    uint32_t sz_root        = (uint32_t)(sizeof(VEA_RES_DIR) + type_dir_count * sizeof(VEA_RES_DIR_ENTRY));

    uint32_t off_level2     = off_root + sz_root;
    uint32_t sz_level2      = (uint32_t)(type_dir_count * sizeof(VEA_RES_DIR) + name_dir_count * sizeof(VEA_RES_DIR_ENTRY));

    uint32_t off_level3     = off_level2 + sz_level2;
    uint32_t sz_level3      = (uint32_t)(name_dir_count * sizeof(VEA_RES_DIR) + gItemCount * sizeof(VEA_RES_DIR_ENTRY));

    uint32_t off_data_ents  = off_level3 + sz_level3;
    uint32_t sz_data_ents   = (uint32_t)(gItemCount * sizeof(VEA_RES_DATA_ENTRY));

    uint32_t off_strings    = off_data_ents + sz_data_ents;
    uint32_t off_blobs      = off_strings + string_pool_bytes;

    uint32_t total_size = off_blobs;
    for (size_t i = 0; i < gItemCount; i++) {
        total_size += (gItems[i].size + 3U) & ~3U;
    }

    uint8_t *buf = calloc(1, total_size);
    if (!buf) return NULL;

    /* Tulis Root Directory */
    VEA_RES_DIR *root_dir = (VEA_RES_DIR *)(buf + off_root);
    VEA_RES_DIR_ENTRY *root_ents = (VEA_RES_DIR_ENTRY *)(root_dir + 1);

    uint32_t cur_l2_dir   = off_level2;
    uint32_t cur_l3_dir   = off_level3;
    uint32_t cur_data_ent = off_data_ents;
    uint32_t cur_str      = off_strings;
    uint32_t cur_blob     = off_blobs;

    size_t t_idx = 0;
    for (size_t i = 0; i < gItemCount; ) {
        if (gItems[i].type_is_str) {
            root_dir->NumberOfNamedEntries++;
            uint16_t slen = (uint16_t)strlen(gItems[i].type_str);
            root_ents[t_idx].Name = VEA_RSRC_NAME_IS_STRING | cur_str;
            memcpy(buf + cur_str, &slen, 2);
            cur_str += 2;
            for (uint16_t c = 0; c < slen; c++) {
                uint16_t u16 = (uint8_t)gItems[i].type_str[c];
                memcpy(buf + cur_str, &u16, 2);
                cur_str += 2;
            }
        } else {
            root_dir->NumberOfIdEntries++;
            root_ents[t_idx].Name = gItems[i].type_id;
        }
        root_ents[t_idx].OffsetToData = VEA_RSRC_DATA_IS_DIRECTORY | cur_l2_dir;
        t_idx++;

        /* Level 2: Name/ID Directory untuk Type ini */
        VEA_RES_DIR *l2_dir = (VEA_RES_DIR *)(buf + cur_l2_dir);
        VEA_RES_DIR_ENTRY *l2_ents = (VEA_RES_DIR_ENTRY *)(l2_dir + 1);
        size_t n_idx = 0;

        size_t j = i;
        while (j < gItemCount && same_type(&gItems[i], &gItems[j])) {
            if (gItems[j].name_is_str) {
                l2_dir->NumberOfNamedEntries++;
                uint16_t slen = (uint16_t)strlen(gItems[j].name_str);
                l2_ents[n_idx].Name = VEA_RSRC_NAME_IS_STRING | cur_str;
                memcpy(buf + cur_str, &slen, 2);
                cur_str += 2;
                for (uint16_t c = 0; c < slen; c++) {
                    uint16_t u16 = (uint8_t)gItems[j].name_str[c];
                    memcpy(buf + cur_str, &u16, 2);
                    cur_str += 2;
                }
            } else {
                l2_dir->NumberOfIdEntries++;
                l2_ents[n_idx].Name = gItems[j].name_id;
            }
            l2_ents[n_idx].OffsetToData = VEA_RSRC_DATA_IS_DIRECTORY | cur_l3_dir;
            n_idx++;

            /* Level 3: Language Directory untuk Name/ID ini */
            VEA_RES_DIR *l3_dir = (VEA_RES_DIR *)(buf + cur_l3_dir);
            VEA_RES_DIR_ENTRY *l3_ents = (VEA_RES_DIR_ENTRY *)(l3_dir + 1);
            size_t l_idx = 0;

            size_t k = j;
            while (k < gItemCount && same_type(&gItems[j], &gItems[k]) && same_name(&gItems[j], &gItems[k])) {
                l3_dir->NumberOfIdEntries++;
                l3_ents[l_idx].Name         = gItems[k].lang_id;
                l3_ents[l_idx].OffsetToData = cur_data_ent; /* Leaf: DataIsDirectory = 0 */
                l_idx++;

                /* Tulis RESOURCE_DATA_ENTRY & Data mentah */
                VEA_RES_DATA_ENTRY *de = (VEA_RES_DATA_ENTRY *)(buf + cur_data_ent);
                de->OffsetToData = cur_blob;
                de->Size         = gItems[k].size;
                de->CodePage     = gItems[k].codepage;
                de->Reserved     = 0;

                if (gItems[k].size > 0) {
                    memcpy(buf + cur_blob, gItems[k].data, gItems[k].size);
                }
                cur_blob     += (gItems[k].size + 3U) & ~3U;
                cur_data_ent += sizeof(VEA_RES_DATA_ENTRY);
                k++;
            }

            cur_l3_dir += (uint32_t)(sizeof(VEA_RES_DIR) + l_idx * sizeof(VEA_RES_DIR_ENTRY));
            j = k;
        }

        cur_l2_dir += (uint32_t)(sizeof(VEA_RES_DIR) + n_idx * sizeof(VEA_RES_DIR_ENTRY));
        i = j;
    }

    *out_total_size = total_size;
    return buf;
}

/* =========================================================================
 * 8. Writer Output ELF32 Relocatable Object (.o) dengan Section .rsrc
 * ========================================================================= */
static bool write_elf32_object(const char *out_path, const uint8_t *rsrc_data, uint32_t rsrc_size) {
    static const char shstrtab[] = "\0.rsrc\0.symtab\0.strtab\0.shstrtab\0";
    static const char strtab[] = "\0VeaResourceStart\0VeaResourceEnd\0";

    Elf32_Sym syms[4];
    memset(syms, 0, sizeof(syms));

    /* [1] Section symbol untuk .rsrc (STB_LOCAL = 0, STT_SECTION = 3) */
    syms[1].st_info  = (0 << 4) | 3;
    syms[1].st_shndx = 1;

    /* [2] VeaResourceStart (STB_GLOBAL = 1, STT_OBJECT = 1) */
    syms[2].st_name  = 1;
    syms[2].st_value = 0;
    syms[2].st_size  = rsrc_size;
    syms[2].st_info  = (1 << 4) | 1;
    syms[2].st_shndx = 1;

    /* [3] VeaResourceEnd (STB_GLOBAL = 1, STT_OBJECT = 1) */
    syms[3].st_name  = 18;
    syms[3].st_value = rsrc_size;
    syms[3].st_size  = 0;
    syms[3].st_info  = (1 << 4) | 1;
    syms[3].st_shndx = 1;

    uint32_t off_rsrc     = 64; /* Align 16 setelah 52-byte ELF header */
    uint32_t off_symtab   = (off_rsrc + rsrc_size + 3U) & ~3U;
    uint32_t off_strtab   = off_symtab + sizeof(syms);
    uint32_t off_shstrtab = off_strtab + sizeof(strtab);
    uint32_t off_shdr     = (off_shstrtab + sizeof(shstrtab) + 3U) & ~3U;

    Elf32_Ehdr ehdr;
    memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[0] = 0x7F; ehdr.e_ident[1] = 'E'; ehdr.e_ident[2] = 'L'; ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = 1; /* ELFCLASS32 */
    ehdr.e_ident[5] = 1; /* ELFDATA2LSB */
    ehdr.e_ident[6] = 1; /* EV_CURRENT */
    ehdr.e_type      = 1; /* ET_REL */
    ehdr.e_machine   = 3; /* EM_386 */
    ehdr.e_version   = 1;
    ehdr.e_shoff     = off_shdr;
    ehdr.e_ehsize    = sizeof(Elf32_Ehdr);
    ehdr.e_shentsize = sizeof(Elf32_Shdr);
    ehdr.e_shnum     = 5;
    ehdr.e_shstrndx  = 4;

    Elf32_Shdr shdrs[5];
    memset(shdrs, 0, sizeof(shdrs));

    /* [1] .rsrc */
    shdrs[1].sh_name      = 1;
    shdrs[1].sh_type      = 1; /* SHT_PROGBITS */
    shdrs[1].sh_flags     = 2; /* SHF_ALLOC (Read-Only) */
    shdrs[1].sh_offset    = off_rsrc;
    shdrs[1].sh_size      = rsrc_size;
    shdrs[1].sh_addralign = 16;

    /* [2] .symtab */
    shdrs[2].sh_name      = 7;
    shdrs[2].sh_type      = 2; /* SHT_SYMTAB */
    shdrs[2].sh_offset    = off_symtab;
    shdrs[2].sh_size      = sizeof(syms);
    shdrs[2].sh_link      = 3; /* .strtab */
    shdrs[2].sh_info      = 2; /* Index symbol global pertama */
    shdrs[2].sh_addralign = 4;
    shdrs[2].sh_entsize   = sizeof(Elf32_Sym);

    /* [3] .strtab */
    shdrs[3].sh_name      = 15;
    shdrs[3].sh_type      = 3; /* SHT_STRTAB */
    shdrs[3].sh_offset    = off_strtab;
    shdrs[3].sh_size      = sizeof(strtab);
    shdrs[3].sh_addralign = 1;

    /* [4] .shstrtab */
    shdrs[4].sh_name      = 23;
    shdrs[4].sh_type      = 3; /* SHT_STRTAB */
    shdrs[4].sh_offset    = off_shstrtab;
    shdrs[4].sh_size      = sizeof(shstrtab);
    shdrs[4].sh_addralign = 1;

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "vearc: gagal menulis '%s'\n", out_path);
        return false;
    }

    uint8_t zero[16] = { 0 };
    fwrite(&ehdr, 1, sizeof(ehdr), f);
    fwrite(zero, 1, off_rsrc - sizeof(ehdr), f);

    fwrite(rsrc_data, 1, rsrc_size, f);
    fwrite(zero, 1, off_symtab - (off_rsrc + rsrc_size), f);

    fwrite(syms, 1, sizeof(syms), f);
    fwrite(strtab, 1, sizeof(strtab), f);
    fwrite(shstrtab, 1, sizeof(shstrtab), f);
    fwrite(zero, 1, off_shdr - (off_shstrtab + sizeof(shstrtab)), f);

    fwrite(shdrs, 1, sizeof(shdrs), f);
    fclose(f);
    return true;
}

int main(int argc, char **argv) {
    const char *in_file  = NULL;
    const char *out_file = "rsrc.o";
    bool raw_mode = false;

    init_builtin_macros();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            if (gIncDirCount < 32) gIncDirs[gIncDirCount++] = argv[++i];
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            if (gIncDirCount < 32) gIncDirs[gIncDirCount++] = argv[i] + 2;
        } else if (strcmp(argv[i], "--raw") == 0) {
            raw_mode = true;
        } else if (argv[i][0] != '-') {
            in_file = argv[i];
        }
    }

    if (!in_file) {
        fprintf(stderr, "Pemakaian: vearc <input.rc> [-I <incdir>] [-o <output.o>] [--raw]\n");
        return 1;
    }

    if (!parse_rc_script(in_file)) {
        return 1;
    }

    uint32_t rsrc_size = 0;
    uint8_t *rsrc_bin = build_rsrc_binary(&rsrc_size);
    if (!rsrc_bin) {
        return 1;
    }

    bool ok = false;
    if (raw_mode) {
        FILE *f = fopen(out_file, "wb");
        if (f) {
            fwrite(rsrc_bin, 1, rsrc_size, f);
            fclose(f);
            ok = true;
        }
    } else {
        ok = write_elf32_object(out_file, rsrc_bin, rsrc_size);
    }

    free(rsrc_bin);
    if (!ok) return 1;

    printf("vearc: %s -> %s (%u bytes .rsrc, %zu entries)\n",
           in_file, out_file, rsrc_size, gItemCount);
    return 0;
}