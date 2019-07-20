#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <cre/cre.h>

enum {
    CRE_TOKEN_LIST,
    CRE_TOKEN_LITERAL,
    CRE_TOKEN_SPECIAL,
    CRE_TOKEN_RANGE,
    CRE_TOKEN_ANCHOR,
    CRE_TOKEN_ALTERNATOR,
    CRE_TOKEN_QUANTIFIER,
    CRE_TOKEN_CAPTURE_GROUP,
};

enum {
    CRE_SPECIAL_DIGIT,
    CRE_SPECIAL_WORD,
    CRE_SPECIAL_WHITESPACE,
};

enum {
    CRE_ANCHOR_START,
    CRE_ANCHOR_END,
    CRE_ANCHOR_START_STRING,
    CRE_ANCHOR_END_STRING,
    CRE_ANCHOR_END_LAST_MATCH,
    CRE_ANCHOR_WORD_BOUNDARY,
};

struct cre_token;
typedef struct cre_token cre_token_t;

struct cre_token {
    int type;
    union {
        struct {
            int count;
            cre_token_t * tokens;
            cre_token_t * parent;
        } list;
        struct {
            bool not;
            char c;
        } literal;
        struct {
            bool not;
            int type;
        } special;
        struct {
            bool not;
            char min;
            char max;
        } range;
        struct {
            int type;
        } anchor;
        struct {
            int type;
            int min;
            int max;
        } quantifier;
    };
};

struct cre_re {
    cre_token_t root;
};

cre_token_t * cre_add_token(cre_token_t * list) {
    cre_token_t * token = NULL;

    ++list->list.count;
    list->list.tokens = (cre_token_t *)realloc(list->list.tokens, sizeof(cre_token_t) * list->list.count);
    token = list->list.tokens + (list->list.count - 1);
    
    memset(token, 0, sizeof(cre_token_t));
    return token;
}

cre_re_t * cre_compile(const char * regex) {
    size_t len = strlen(regex);
    if (len == 0) {
        return NULL;
    }

    cre_re_t * re;
    re = (cre_re_t *)malloc(sizeof(cre_re_t));
    re->root.type = CRE_TOKEN_LIST;
    re->root.list.count = 0;
    re->root.list.tokens = NULL;
    re->root.list.parent = NULL;

    bool in_escape = false;
    bool in_capture_group = false;
    bool in_alternator = false;
    bool in_quantifier = false;

    int capture_group_count = 0;
    cre_token_t ** capture_groups = NULL;

    char strnum[32] = { '\0' };

    cre_token_t * parent = &(re->root);
    cre_token_t * token = NULL;
    for (int i = 0; i < len; ++i) {
        char c = regex[i];
        char nc = (i < len - 1 ? regex[i + 1] : '\0');
        char nnc = (i < len - 2 ? regex[i + 2] : '\0');

        if (in_escape) {
            switch (c) {
            case '\\':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '+':
            case '.':
            case '?':
            case '*':
            case '^':
            case '$':
            case '|':
                token = cre_add_token(parent);
                token->type = CRE_TOKEN_LITERAL;
                token->literal.c = c;
                break;
            case 't':
                token = cre_add_token(parent);
                token->type = CRE_TOKEN_LITERAL;
                token->literal.c = '\t';
                break;
            case 'n':
                token = cre_add_token(parent);
                token->type = CRE_TOKEN_LITERAL;
                token->literal.c = '\n';
                break;
            case 'r':
                token = cre_add_token(parent);
                token->type = CRE_TOKEN_LITERAL;
                token->literal.c = '\r';
                break;
            case 'd':
                token = cre_add_token(parent);
                token->type = CRE_TOKEN_SPECIAL;
                token->special.type = CRE_SPECIAL_DIGIT;
                break;
            }

            in_escape = false;
            continue;
        }

        if (in_alternator) {
            if (nc == '-') {
                if ((c >= 'a' && c <= 'z' && nnc >= 'a' && nnc <= 'z') ||
                    (c >= 'A' && c <= 'Z' && nnc >= 'A' && nnc <= 'Z') ||
                    (c >= '0' && c <= '9' && nnc >= '0' && nnc <= '9')) {
                    token = cre_add_token(parent);
                    token->type = CRE_TOKEN_RANGE;
                    token->range.min = c;
                    token->range.max = nnc;

                    i += 2;
                    continue;
                }
            }
        }

        if (in_quantifier) {
            if (c == '}') {
                in_quantifier = false;
            }

            continue;
        }

        switch (c) {
        case '\\':
            in_escape = true;
            break;
        case '+':
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_QUANTIFIER;
            token->quantifier.min = 1;
            token->quantifier.max = -1;
            break;
        case '?':
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_QUANTIFIER;
            token->quantifier.min = 0;
            token->quantifier.max = 1;
            break;
        case '*':
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_QUANTIFIER;
            token->quantifier.min = 0;
            token->quantifier.max = -1;
            break;
        case '(':
            in_capture_group = true;
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_CAPTURE_GROUP;
            token->list.count = 0;
            token->list.tokens = NULL;
            token->list.parent = parent;
            parent = token;
            break;
        case ')':
            in_capture_group = false;
            parent = parent->list.parent;
            break;
        case '[':
            in_alternator = true;
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_ALTERNATOR;
            token->list.count = 0;
            token->list.tokens = NULL;
            token->list.parent = parent;
            parent = token;
            break;
        case ']':
            in_capture_group = false;
            parent = parent->list.parent;
            break;
        case '{':
            in_quantifier = true;
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_QUANTIFIER;
            token->quantifier.min = -1;
            token->quantifier.max = -1;
            if (sscanf(regex + i + 1, "%d,%d}", &(token->quantifier.min), &(token->quantifier.max)) < 2) {
                token->quantifier.max = token->quantifier.min;
            }
            break;
        default:
            token = cre_add_token(parent);
            token->type = CRE_TOKEN_LITERAL;
            token->literal.c = c;
            break;
        };
    }

    return re;
}

void cre_token_free(cre_token_t * token) {
    switch (token->type) {
    case CRE_TOKEN_LIST:
    case CRE_TOKEN_ALTERNATOR:
    case CRE_TOKEN_CAPTURE_GROUP:
        for (int i = 0; i < token->list.count; ++i) {
            cre_token_free(token->list.tokens + i);
        }
        free(token->list.tokens);
        break;
    };
}

void cre_free(cre_re_t * re) {
    cre_token_free(&(re->root));
    free(re);
}

void print_indent(int depth, bool first) {
    if (depth == 0) {
        return;
    }
    
    for (int i = 0; i < depth - 1; ++i) {
        printf("|  ");
    }
    if (first) {
        printf("├─ ");
    } else {
        printf("|  ");
    }
}

void cre_token_print(cre_token_t * token, int depth) {
    switch (token->type) {
    case CRE_TOKEN_LITERAL:
        print_indent(depth, true);
        printf("type: literal\n");
        print_indent(depth, false);
        printf("not: %s\n", (token->literal.not ? "true" : "false"));
        print_indent(depth, false);
        printf("c: %c\n", token->literal.c);
        break;
    case CRE_TOKEN_QUANTIFIER:
        print_indent(depth, true);
        printf("type: quantifier\n");
        print_indent(depth, false);
        printf("min: %d\n", token->quantifier.min);
        print_indent(depth, false);
        printf("max: %d\n", token->quantifier.max);
        break;
    case CRE_TOKEN_SPECIAL:
        print_indent(depth, true);
        printf("type: literal\n");
        print_indent(depth, false);
        printf("not: %s\n", (token->special.not ? "true" : "false"));
        print_indent(depth, false);
        switch (token->special.type) {
        case CRE_SPECIAL_DIGIT:
            printf("digit\n");
        case CRE_SPECIAL_WORD:
            printf("word\n");
        case CRE_SPECIAL_WHITESPACE:
            printf("whitespace\n");
        }
        break;
    case CRE_TOKEN_RANGE:
        print_indent(depth, true);
        printf("type: range\n");
        print_indent(depth, false);
        printf("min: %c\n", token->range.min);
        print_indent(depth, false);
        printf("max: %c\n", token->range.max);
        break;
    case CRE_TOKEN_LIST:
        print_indent(depth, true);
        printf("type: list\n");
        print_indent(depth, false);
        printf("tokens:\n");
        for (int i = 0; i < token->list.count; ++i) {
            cre_token_print(token->list.tokens + i, depth + 1);
        }
        break;
    case CRE_TOKEN_ALTERNATOR:
        print_indent(depth, true);
        printf("type: alternator\n");
        print_indent(depth, false);
        printf("tokens:\n");
        for (int i = 0; i < token->list.count; ++i) {
            cre_token_print(token->list.tokens + i, depth + 1);
        }
        break;
    case CRE_TOKEN_CAPTURE_GROUP:
        print_indent(depth, true);
        printf("type: capture group\n");
        print_indent(depth, false);
        printf("tokens:\n");
        for (int i = 0; i < token->list.count; ++i) {
            cre_token_print(token->list.tokens + i, depth + 1);
        }
        break;
    }
}

void cre_print(cre_re_t * re) {
    cre_token_print(&(re->root), 0);
}