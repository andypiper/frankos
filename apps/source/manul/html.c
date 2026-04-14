/*
 * Manul — Text Web Browser for FRANK OS
 * HTML tokeniser/parser (ported from frank-lynx)
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * All switch statements replaced with if/else chains
 * (switch is redefined as DO_NOT_USE_SWITCH in m-os-api.h).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "manul.h"
#include "html.h"

/* ------------------------------------------------------------------
 * Void (self-closing) HTML elements — no closing tag expected
 * ------------------------------------------------------------------ */
static bool is_void_element(const char *tag)
{
    if (strcmp(tag, "area") == 0) return true;
    if (strcmp(tag, "base") == 0) return true;
    if (strcmp(tag, "br") == 0) return true;
    if (strcmp(tag, "col") == 0) return true;
    if (strcmp(tag, "embed") == 0) return true;
    if (strcmp(tag, "hr") == 0) return true;
    if (strcmp(tag, "img") == 0) return true;
    if (strcmp(tag, "input") == 0) return true;
    if (strcmp(tag, "link") == 0) return true;
    if (strcmp(tag, "meta") == 0) return true;
    if (strcmp(tag, "param") == 0) return true;
    if (strcmp(tag, "source") == 0) return true;
    if (strcmp(tag, "track") == 0) return true;
    if (strcmp(tag, "wbr") == 0) return true;
    return false;
}

/* ------------------------------------------------------------------
 * HTML entity decoding  (named + numeric decimal/hex, UTF-8 output)
 * Returns number of bytes written to out, advances *src past the ';'
 * ------------------------------------------------------------------ */
static int decode_entity(const char *src, const char *end, char *out,
                         const char **next)
{
    const char *p = src;      /* points at '&' */
    p++;                      /* skip '&' */
    if (p >= end) { *next = src; return 0; }

    /* find ';' within a reasonable range */
    const char *semi = p;
    int maxscan = 12;
    while (semi < end && *semi != ';' && maxscan > 0) {
        semi++;
        maxscan--;
    }
    if (semi >= end || *semi != ';') {
        /* not a valid entity — emit '&' literally */
        *next = src + 1;
        out[0] = '&';
        return 1;
    }

    uint16_t elen = (uint16_t)(semi - p);  /* length of entity name/number */

    /* --- numeric entities ------------------------------------------------ */
    if (*p == '#') {
        p++;
        uint32_t cp = 0;
        if (*p == 'x' || *p == 'X') {
            /* hex */
            p++;
            while (p < semi) {
                char c = manul_tolower(*p);
                if (c >= '0' && c <= '9')      cp = cp * 16 + (uint32_t)(c - '0');
                else if (c >= 'a' && c <= 'f') cp = cp * 16 + (uint32_t)(c - 'a' + 10);
                else break;
                p++;
            }
        } else {
            /* decimal */
            while (p < semi) {
                if (*p >= '0' && *p <= '9') cp = cp * 10 + (uint32_t)(*p - '0');
                else break;
                p++;
            }
        }
        *next = semi + 1;
        /* encode code point as UTF-8 */
        if (cp < 0x80) {
            out[0] = (char)cp;
            return 1;
        } else if (cp < 0x800) {
            out[0] = (char)(0xC0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3F));
            return 2;
        } else if (cp < 0x10000) {
            out[0] = (char)(0xE0 | (cp >> 12));
            out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[2] = (char)(0x80 | (cp & 0x3F));
            return 3;
        } else if (cp < 0x110000) {
            out[0] = (char)(0xF0 | (cp >> 18));
            out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[3] = (char)(0x80 | (cp & 0x3F));
            return 4;
        }
        /* invalid code point */
        out[0] = '?';
        return 1;
    }

    /* --- named entities -------------------------------------------------- */
    *next = semi + 1;

    if (elen == 2 && memcmp(p, "lt", 2) == 0) { out[0] = '<'; return 1; }
    if (elen == 2 && memcmp(p, "gt", 2) == 0) { out[0] = '>'; return 1; }
    if (elen == 3 && memcmp(p, "amp", 3) == 0) { out[0] = '&'; return 1; }
    if (elen == 4 && memcmp(p, "quot", 4) == 0) { out[0] = '"'; return 1; }
    if (elen == 4 && memcmp(p, "apos", 4) == 0) { out[0] = '\''; return 1; }
    if (elen == 4 && memcmp(p, "nbsp", 4) == 0) { out[0] = ' '; return 1; }
    if (elen == 5 && memcmp(p, "mdash", 5) == 0) {
        /* U+2014 em dash -> UTF-8 E2 80 94 */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x94;
        return 3;
    }
    if (elen == 5 && memcmp(p, "ndash", 5) == 0) {
        /* U+2013 en dash -> UTF-8 E2 80 93 */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x93;
        return 3;
    }
    if (elen == 4 && memcmp(p, "bull", 4) == 0) {
        /* U+2022 bullet -> UTF-8 E2 80 A2 */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0xA2;
        return 3;
    }
    if (elen == 5 && memcmp(p, "laquo", 5) == 0) {
        /* U+00AB << */
        out[0] = (char)0xC2; out[1] = (char)0xAB;
        return 2;
    }
    if (elen == 5 && memcmp(p, "raquo", 5) == 0) {
        /* U+00BB >> */
        out[0] = (char)0xC2; out[1] = (char)0xBB;
        return 2;
    }
    if (elen == 4 && memcmp(p, "copy", 4) == 0) {
        /* U+00A9 (c) */
        out[0] = (char)0xC2; out[1] = (char)0xA9;
        return 2;
    }
    if (elen == 3 && memcmp(p, "reg", 3) == 0) {
        /* U+00AE (R) */
        out[0] = (char)0xC2; out[1] = (char)0xAE;
        return 2;
    }
    if (elen == 5 && memcmp(p, "trade", 5) == 0) {
        /* U+2122 TM */
        out[0] = (char)0xE2; out[1] = (char)0x84; out[2] = (char)0xA2;
        return 3;
    }
    if (elen == 6 && memcmp(p, "hellip", 6) == 0) {
        /* U+2026 ... */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0xA6;
        return 3;
    }
    if (elen == 5 && memcmp(p, "rsquo", 5) == 0) {
        /* U+2019 right single quote */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x99;
        return 3;
    }
    if (elen == 5 && memcmp(p, "lsquo", 5) == 0) {
        /* U+2018 left single quote */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x98;
        return 3;
    }
    if (elen == 5 && memcmp(p, "rdquo", 5) == 0) {
        /* U+201D right double quote */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x9D;
        return 3;
    }
    if (elen == 5 && memcmp(p, "ldquo", 5) == 0) {
        /* U+201C left double quote */
        out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0x9C;
        return 3;
    }

    /* unknown entity — emit '?' */
    out[0] = '?';
    return 1;
}

/* ------------------------------------------------------------------
 * Flush pending text token
 * ------------------------------------------------------------------ */
static void flush_text(html_parser_t *p, html_token_cb_t cb, void *ctx)
{
    if (p->token.text_len == 0) return;
    p->token.type = HTML_TOKEN_TEXT;
    p->token.tag[0] = '\0';
    p->token.text[p->token.text_len] = '\0';
    cb(&p->token, ctx);
    p->token.text_len = 0;
}

/* ------------------------------------------------------------------
 * Append a character to the pending text buffer, decoding entities
 * ------------------------------------------------------------------ */
static void text_append_char(html_parser_t *p, char c)
{
    if (p->token.text_len < sizeof(p->token.text) - 1) {
        p->token.text[p->token.text_len++] = c;
    }
}

/* ------------------------------------------------------------------
 * Reset token attribute fields
 * ------------------------------------------------------------------ */
static void clear_token_attrs(html_token_t *t)
{
    t->attr_href[0] = '\0';
    t->attr_alt[0] = '\0';
    t->attr_type[0] = '\0';
    t->attr_name[0] = '\0';
    t->attr_value[0] = '\0';
    t->attr_action[0] = '\0';
    t->attr_src[0] = '\0';
}

/* ------------------------------------------------------------------
 * Parse attributes from the raw buffer p->buf (already accumulated)
 * Fills the token's attr_* fields.
 * ------------------------------------------------------------------ */
static void parse_attributes(html_parser_t *p)
{
    clear_token_attrs(&p->token);
    const char *s = p->buf;
    const char *end = p->buf + p->buf_len;

    while (s < end) {
        /* skip whitespace */
        while (s < end && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
            s++;
        if (s >= end) break;

        /* collect attribute name */
        const char *name_start = s;
        while (s < end && *s != '=' && *s != ' ' && *s != '\t' &&
               *s != '\n' && *s != '\r' && *s != '>' && *s != '/')
            s++;
        uint16_t name_len = (uint16_t)(s - name_start);
        if (name_len == 0) { s++; continue; }

        /* lowercase the name into a small buffer */
        char aname[32];
        uint16_t copy_len = name_len < sizeof(aname) - 1
                            ? name_len : (uint16_t)(sizeof(aname) - 1);
        for (uint16_t i = 0; i < copy_len; i++)
            aname[i] = manul_tolower(name_start[i]);
        aname[copy_len] = '\0';

        /* skip whitespace around '=' */
        while (s < end && (*s == ' ' || *s == '\t')) s++;
        if (s >= end || *s != '=') {
            /* attribute with no value — skip */
            continue;
        }
        s++;  /* skip '=' */
        while (s < end && (*s == ' ' || *s == '\t')) s++;
        if (s >= end) break;

        /* collect attribute value */
        char aval[128];
        uint16_t vlen = 0;
        if (*s == '"' || *s == '\'') {
            char quote = *s++;
            while (s < end && *s != quote && vlen < sizeof(aval) - 1)
                aval[vlen++] = *s++;
            if (s < end) s++;  /* skip closing quote */
        } else {
            while (s < end && *s != ' ' && *s != '\t' && *s != '>' &&
                   *s != '\n' && *s != '\r' && vlen < sizeof(aval) - 1)
                aval[vlen++] = *s++;
        }
        aval[vlen] = '\0';

        /* store into the correct token field */
        if (strcmp(aname, "href") == 0) {
            strncpy(p->token.attr_href, aval, sizeof(p->token.attr_href) - 1);
            p->token.attr_href[sizeof(p->token.attr_href) - 1] = '\0';
        } else if (strcmp(aname, "alt") == 0) {
            strncpy(p->token.attr_alt, aval, sizeof(p->token.attr_alt) - 1);
            p->token.attr_alt[sizeof(p->token.attr_alt) - 1] = '\0';
        } else if (strcmp(aname, "type") == 0) {
            strncpy(p->token.attr_type, aval, sizeof(p->token.attr_type) - 1);
            p->token.attr_type[sizeof(p->token.attr_type) - 1] = '\0';
        } else if (strcmp(aname, "name") == 0) {
            strncpy(p->token.attr_name, aval, sizeof(p->token.attr_name) - 1);
            p->token.attr_name[sizeof(p->token.attr_name) - 1] = '\0';
        } else if (strcmp(aname, "value") == 0) {
            strncpy(p->token.attr_value, aval, sizeof(p->token.attr_value) - 1);
            p->token.attr_value[sizeof(p->token.attr_value) - 1] = '\0';
        } else if (strcmp(aname, "action") == 0) {
            strncpy(p->token.attr_action, aval, sizeof(p->token.attr_action) - 1);
            p->token.attr_action[sizeof(p->token.attr_action) - 1] = '\0';
        } else if (strcmp(aname, "src") == 0) {
            strncpy(p->token.attr_src, aval, sizeof(p->token.attr_src) - 1);
            p->token.attr_src[sizeof(p->token.attr_src) - 1] = '\0';
        }
    }
}

/* ------------------------------------------------------------------
 * Emit a tag token (open, close, or self-close)
 * ------------------------------------------------------------------ */
static void emit_tag(html_parser_t *p, html_token_cb_t cb, void *ctx)
{
    /* lowercase the tag name */
    for (uint16_t i = 0; i < p->tag_name_len; i++)
        p->current_tag[i] = manul_tolower(p->current_tag[i]);
    p->current_tag[p->tag_name_len] = '\0';

    strncpy(p->token.tag, p->current_tag, sizeof(p->token.tag) - 1);
    p->token.tag[sizeof(p->token.tag) - 1] = '\0';
    p->token.text[0] = '\0';
    p->token.text_len = 0;

    if (p->is_closing_tag) {
        p->token.type = HTML_TOKEN_TAG_CLOSE;
        clear_token_attrs(&p->token);
    } else {
        parse_attributes(p);
        if (is_void_element(p->current_tag)) {
            p->token.type = HTML_TOKEN_TAG_SELF_CLOSE;
        } else {
            p->token.type = HTML_TOKEN_TAG_OPEN;
        }
    }

    cb(&p->token, ctx);

    /* enter script/style skip mode */
    if (!p->is_closing_tag) {
        if (strcmp(p->current_tag, "script") == 0)
            p->state = HPS_SCRIPT;
        else if (strcmp(p->current_tag, "style") == 0)
            p->state = HPS_STYLE;
    }
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

void html_parser_init(html_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = HPS_TEXT;
}

void html_parser_feed(html_parser_t *p, const uint8_t *data, uint16_t len,
                      html_token_cb_t cb, void *ctx)
{
    const uint8_t *end = data + len;

    for (const uint8_t *dp = data; dp < end; dp++) {
        char c = (char)*dp;

        /* ============================================================
         * State: HPS_TEXT  — collecting visible text
         * ============================================================ */
        if (p->state == HPS_TEXT) {
            if (c == '<') {
                /* flush accumulated text before opening a tag */
                flush_text(p, cb, ctx);
                p->state = HPS_TAG_NAME;
                p->is_closing_tag = false;
                p->tag_name_len = 0;
                p->buf_len = 0;
                p->current_tag[0] = '\0';
            } else if (c == '&') {
                /* entity reference in text */
                const char *src_ptr = (const char *)dp;
                const char *src_end = (const char *)end;
                const char *next = src_ptr;
                char decoded[5];
                int n = decode_entity(src_ptr, src_end, decoded, &next);
                for (int i = 0; i < n; i++)
                    text_append_char(p, decoded[i]);
                dp = (const uint8_t *)next - 1;  /* -1 because loop increments */
            } else {
                /* collapse whitespace: \r \n \t -> space */
                if (c == '\r' || c == '\n' || c == '\t')
                    c = ' ';
                text_append_char(p, c);
            }
        }

        /* ============================================================
         * State: HPS_TAG_NAME  — reading tag name after '<'
         * ============================================================ */
        else if (p->state == HPS_TAG_NAME) {
            if (c == '!' && p->tag_name_len == 0) {
                /* could be comment <!-- or DOCTYPE */
                p->state = HPS_COMMENT;
                p->buf[0] = '!';
                p->buf_len = 1;
            } else if (c == '/' && p->tag_name_len == 0) {
                p->is_closing_tag = true;
            } else if (c == '>' || c == ' ' || c == '\t' ||
                       c == '\n' || c == '\r') {
                if (p->tag_name_len == 0 && c == '>') {
                    /* empty tag <> — just go back to text */
                    p->state = HPS_TEXT;
                } else if (c == '>') {
                    /* end of tag, no attributes */
                    p->buf_len = 0;
                    emit_tag(p, cb, ctx);
                    if (p->state != HPS_SCRIPT && p->state != HPS_STYLE)
                        p->state = HPS_TEXT;
                } else {
                    /* whitespace — switch to attribute accumulation */
                    p->state = HPS_TAG_ATTRS;
                    p->buf_len = 0;
                }
            } else if (c == '/') {
                /* self-closing slash before '>' e.g. <br/> */
                /* keep reading, will close on '>' */
            } else {
                if (p->tag_name_len < sizeof(p->current_tag) - 1) {
                    p->current_tag[p->tag_name_len++] = c;
                }
            }
        }

        /* ============================================================
         * State: HPS_TAG_ATTRS  — accumulating raw attribute text
         * ============================================================ */
        else if (p->state == HPS_TAG_ATTRS) {
            if (c == '>') {
                p->buf[p->buf_len] = '\0';
                emit_tag(p, cb, ctx);
                if (p->state != HPS_SCRIPT && p->state != HPS_STYLE)
                    p->state = HPS_TEXT;
            } else {
                if (p->buf_len < sizeof(p->buf) - 1) {
                    p->buf[p->buf_len++] = c;
                }
            }
        }

        /* ============================================================
         * State: HPS_COMMENT  — skip <!-- ... --> and <!DOCTYPE ...>
         * ============================================================ */
        else if (p->state == HPS_COMMENT) {
            if (p->buf_len < sizeof(p->buf) - 1) {
                p->buf[p->buf_len++] = c;
            }
            /* check for end of comment --> */
            if (p->buf_len >= 4 &&
                p->buf[0] == '!' && p->buf[1] == '-' && p->buf[2] == '-') {
                /* we're inside a real comment, look for --> */
                if (p->buf_len >= 3 &&
                    p->buf[p->buf_len - 3] == '-' &&
                    p->buf[p->buf_len - 2] == '-' &&
                    p->buf[p->buf_len - 1] == '>') {
                    p->state = HPS_TEXT;
                }
            } else if (c == '>') {
                /* <!DOCTYPE ...> or malformed comment */
                p->state = HPS_TEXT;
            }
        }

        /* ============================================================
         * State: HPS_SCRIPT  — skip until </script>
         * ============================================================ */
        else if (p->state == HPS_SCRIPT) {
            /* accumulate into buf looking for </script> */
            if (c == '<') {
                p->buf_len = 0;
                p->buf[p->buf_len++] = '<';
            } else if (p->buf_len > 0) {
                if (p->buf_len < sizeof(p->buf) - 1) {
                    p->buf[p->buf_len++] = c;
                }
                if (c == '>') {
                    p->buf[p->buf_len] = '\0';
                    /* check for </script> (case-insensitive) */
                    if (p->buf_len >= 9) {
                        char tmp[10];
                        for (int i = 0; i < 9 && i < (int)p->buf_len; i++)
                            tmp[i] = manul_tolower(p->buf[i]);
                        tmp[9] = '\0';
                        if (memcmp(tmp, "</script>", 9) == 0) {
                            /* emit closing tag */
                            p->is_closing_tag = true;
                            p->tag_name_len = 6;
                            memcpy(p->current_tag, "script", 6);
                            p->current_tag[6] = '\0';
                            p->buf_len = 0;
                            p->token.type = HTML_TOKEN_TAG_CLOSE;
                            strncpy(p->token.tag, "script",
                                    sizeof(p->token.tag) - 1);
                            p->token.tag[sizeof(p->token.tag) - 1] = '\0';
                            clear_token_attrs(&p->token);
                            p->token.text[0] = '\0';
                            p->token.text_len = 0;
                            cb(&p->token, ctx);
                            p->state = HPS_TEXT;
                        }
                    }
                    if (p->state != HPS_TEXT) {
                        p->buf_len = 0;
                    }
                }
            }
        }

        /* ============================================================
         * State: HPS_STYLE  — skip until </style>
         * ============================================================ */
        else if (p->state == HPS_STYLE) {
            if (c == '<') {
                p->buf_len = 0;
                p->buf[p->buf_len++] = '<';
            } else if (p->buf_len > 0) {
                if (p->buf_len < sizeof(p->buf) - 1) {
                    p->buf[p->buf_len++] = c;
                }
                if (c == '>') {
                    p->buf[p->buf_len] = '\0';
                    if (p->buf_len >= 8) {
                        char tmp[9];
                        for (int i = 0; i < 8 && i < (int)p->buf_len; i++)
                            tmp[i] = manul_tolower(p->buf[i]);
                        tmp[8] = '\0';
                        if (memcmp(tmp, "</style>", 8) == 0) {
                            p->is_closing_tag = true;
                            p->tag_name_len = 5;
                            memcpy(p->current_tag, "style", 5);
                            p->current_tag[5] = '\0';
                            p->buf_len = 0;
                            p->token.type = HTML_TOKEN_TAG_CLOSE;
                            strncpy(p->token.tag, "style",
                                    sizeof(p->token.tag) - 1);
                            p->token.tag[sizeof(p->token.tag) - 1] = '\0';
                            clear_token_attrs(&p->token);
                            p->token.text[0] = '\0';
                            p->token.text_len = 0;
                            cb(&p->token, ctx);
                            p->state = HPS_TEXT;
                        }
                    }
                    if (p->state != HPS_TEXT) {
                        p->buf_len = 0;
                    }
                }
            }
        }
    }
}

void html_parser_finish(html_parser_t *p, html_token_cb_t cb, void *ctx)
{
    /* flush any remaining text */
    flush_text(p, cb, ctx);
}
