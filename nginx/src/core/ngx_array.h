
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 *
 * @动态数组@
 * ngx_array_t是一个顺序容器,以数组的形式存储元素,并支持动态改变数组大小
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    void        *elts;   /* 指向数组的首地址 */
    ngx_uint_t   nelts;  /* 数组中已经使用的元素个数 */
    size_t       size;   /* 给个数组元素占用内存的大小 */
    ngx_uint_t   nalloc; /* 当前数组中能够容纳元素的总个数 */
    ngx_pool_t  *pool;   /* 内存池对象,动态数组负责内存的申请 */
} ngx_array_t;

/* 创建动态数组 */
ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_array_destroy(ngx_array_t *a);
void *ngx_array_push(ngx_array_t *a);
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);


static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#define ngx_array_get(_arr, _index) ((u_char *)(_arr)->etls + (_arr)->size * (_index))


#endif /* _NGX_ARRAY_H_INCLUDED_ */
