#ifndef HTML_H
#define HTML_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HTML_TOKEN_TEXT,
    HTML_TOKEN_TAG_OPEN,
    HTML_TOKEN_TAG_CLOSE,
    HTML_TOKEN_TAG_SELF_CLOSE,
} html_token_type_t;

typedef struct {
    html_token_type_t type;
    char tag[16];
    char text[256];
    uint16_t text_len;
    char attr_href[128];
    char attr_alt[64];
    char attr_type[16];
    char attr_name[32];
    char attr_value[64];
    char attr_action[128];
    char attr_src[128];
} html_token_t;

typedef struct {
    int state;  /* use int instead of anonymous enum to avoid switch issues */
    char buf[512];
    uint16_t buf_len;
    bool is_closing_tag;
    char current_tag[16];
    uint16_t tag_name_len;
    html_token_t token;
} html_parser_t;

#define HPS_TEXT       0
#define HPS_TAG_NAME   1
#define HPS_TAG_ATTRS  2
#define HPS_TAG_CLOSE  3
#define HPS_COMMENT    4
#define HPS_SCRIPT     5
#define HPS_STYLE      6

typedef void (*html_token_cb_t)(const html_token_t *token, void *ctx);

void html_parser_init(html_parser_t *p);
void html_parser_feed(html_parser_t *p, const uint8_t *data, uint16_t len,
                      html_token_cb_t cb, void *ctx);
void html_parser_finish(html_parser_t *p, html_token_cb_t cb, void *ctx);

#endif
