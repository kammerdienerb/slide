#ifndef __PRESENTATION_H__
#define __PRESENTATION_H__

#include "internal.h"
#include "array.h"
#include "tree.h"

#define PRES_PARA   (1)
#define PRES_BREAK  (2)
#define PRES_VSPACE (3)
#define PRES_VFILL  (4)
#define PRES_POINT  (5)

typedef struct {
    int      kind;
    int      x, y;
    array_t  text;
    u32      font_id;
    u32      font_size;
    u32      r, g, b;
    u32      l_margin, r_margin;
} pres_elem_t;

typedef char *macro_name_t;
use_tree(macro_name_t, array_t);
typedef tree(macro_name_t, array_t)    macro_map_t;
typedef tree_it(macro_name_t, array_t) macro_map_it;

typedef struct {
    array_t      elements;
    array_t      fonts;
    macro_map_t  macros;
    array_t     *collect_lines;
    u32          r, g, b;
    float        speed;
} pres_t;

pres_t build_presentation(const char *path);
void free_presentation(pres_t *pres);
char * pres_get_font_name_by_id(pres_t *pres, u32 id);

#endif
