#include <engine.h>
#include <tf_header.h>
#include <db_loader.h>
#include <hs_header.h>

typedef enum {
    DECODE_URI    =  0x01,
    DECODE_HTML   =  0x02,
    DECODE_JS     =  0x04,
    DECODE_CSS    =  0x08,
    DECODE_BASE64 =  0x10,
    DELETE_SPACE  =  0x20,
    COMPRESS_SPACE = 0x40,
    DELETE_COMMENT = 0x80
} decode_flags_t;

const static int g_decode_map[] = {
    /* HTTP_FIELD_URL */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS | COMPRESS_SPACE,
    /* HTTP_FIELD_QUERY_ARG_KEY */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_QUERY_ARG_VAL */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS | DELETE_COMMENT,
    /* HTTP_FIELD_POST_ARG_KEY */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_POST_ARG_VAL */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS| DELETE_COMMENT,
    /* HTTP_FIELD_COOKIE_KEY */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_COOKIE_VAL */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS| DELETE_COMMENT,
    /* HTTP_FIELD_HOST */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS | DECODE_BASE64,
    /* HTTP_FILED_REFERER */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_USER_AGENT */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_CONTENT_VAL */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS,
    /* HTTP_FIELD_CONTENT_LENGTH */
    DECODE_URI | DECODE_HTML | DECODE_JS | DECODE_CSS
};

struct _engine_mgt {
    db_mgt_t*  db;
    hs_mgt_t*  hs;
    WHITELIST_CB wcb;    
    risk_level_t risk_level;
};



/* load signatures */
engine_mgt_t*  
waf_engine_init(void)
{
    engine_mgt_t* mgt = calloc(sizeof(engine_mgt_t), 1);
    if (!mgt) {
        perror("calloc failed, ");
        return NULL;
    }

    return mgt;
}

/* if error occurred, we can still use the old database */
int 
waf_engine_set_database(engine_mgt_t* mgt, const char* path)
{
    int ret;
    db_mgt_t*  db;
    hs_mgt_t*  hs;

    ret = signature_database_open(path, &db);
    if (ret != 0) {
        return ret;
    }

    ret = hs_wrapper_create(db, &hs);
    if (ret != 0) {
        return ret;
    }

    hs_wrapper_level(hs, mgt->risk_level);
    hs_wrapper_whitelist(mgt->hs, mgt->wcb);

    /* free old database */
    if (mgt->hs) { /* reload */
        hs_wrapper_destroy(mgt->hs);
        mgt->hs = NULL;
        signature_database_close(mgt->db);
        mgt->db = NULL;
    }

    /* set new database */
    mgt->hs = hs;
    mgt->db = db;

    return 0;
}



int 
waf_engine_set_level(engine_mgt_t* mgt, risk_level_t level)
{
    if (mgt) {
        mgt->risk_level = level;
        if (mgt->hs) {
            hs_wrapper_level(mgt->hs, level);
        }

        return 0;
    }

    return -1;
}



int 
waf_engine_set_whitelist_cb(engine_mgt_t* mgt, WHITELIST_CB cb)
{
    if (mgt) {
        mgt->wcb = cb;
        if (mgt->hs) {
            hs_wrapper_whitelist(mgt->hs, cb);
        }

        return 0;
    }

    return -1;
}



/* scanning */
const signature_info_t*   
waf_engine_scan(engine_mgt_t* mgt, http_field_t field, uint8_t* data, size_t len)
{
    int map = g_decode_map[field];
    signature_info_t* info;

    if (map & DECODE_URI) {
        data = tf_url_decode(data, &len);
    }

    if (map & DECODE_HTML) {
        data = tf_html_entity_decode(data, &len);
    }

    if (map & DECODE_JS) {
        data = tf_js_decode(data, &len);
    }

    if (map & DECODE_CSS) {
        data = tf_css_decode(data, &len);
    }
    
    data = ts_compress_space(data, &len);
    info = hs_wrapper_scan(mgt->hs, data, len);
    if (!info) {
        data = ts_delete_space(data, &len);
        info = hs_wrapper_scan(mgt->hs, data, len);
        if (!info) {
            if (HTTP_FIELD_URL == field) {
                data = tf_base64url_decode(data, &len);
            } else {
                data = tf_base64_decode(data, &len);
            }
        }
        info = hs_wrapper_scan(mgt->hs, data, len);
    }

    return info;
}


/* cleanup */
int  
waf_engine_exit(engine_mgt_t* mgt)
{
    if (mgt) {
        hs_wrapper_destroy(mgt->hs);
        signature_database_close(mgt->db);
        free(mgt);
    }

    return 0;
}


