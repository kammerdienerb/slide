#include "pdf.h"
#include "tree.h"
#include "hpdf.h"

use_tree(font_name_t, HPDF_Font);
use_tree(image_path_t, HPDF_Image);

typedef struct {
    pres_t                         *pres;
    HPDF_Doc                        doc;
    HPDF_Page                       cur_page;
    tree(font_name_t, HPDF_Font)    fonts;
    tree(image_path_t, HPDF_Image)  images;
} pdf_t;

static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    ERR("pdf: error_no=0x%04X, detail_no=%d\n", (unsigned int) error_no, (int) detail_no);
}

static HPDF_Font get_pdf_font(pdf_t *pdf, const char *path) {
    const char *font_name;
    HPDF_Font   font;

    tree_it(font_name_t, HPDF_Font) it;

    it = tree_lookup(pdf->fonts, (char*)path);
    if (tree_it_good(it)) {
        font = tree_it_val(it);
        goto out;
    }

    font_name = HPDF_LoadTTFontFromFile(pdf->doc, path, HPDF_TRUE);
    font      = HPDF_GetFont(pdf->doc, font_name, "UTF-8");

    tree_insert(pdf->fonts, (char*)path, font);

out:;
    return font;
}

static HPDF_Image get_image(pdf_t *pdf, const char *path) {
    const char *ext;
    HPDF_Image  image = NULL;

    tree_it(image_path_t, HPDF_Image) it;

    it = tree_lookup(pdf->images, (char*)path);
    if (tree_it_good(it)) {
        image = tree_it_val(it);
        goto out;
    }

    ext = path + strlen(path);
    for (;;) {
        if (*ext == '.') {
            ext += 1;
            break;
        }
        if (ext < path) {
            ERR("pdf: image path has no extension\n");
        }
        ext -= 1;
    }

    if (strcmp(ext, "png") == 0
    ||  strcmp(ext, "PNG") == 0) {

        image = HPDF_LoadPngImageFromFile(pdf->doc, path);
    } else if (strcmp(ext, "jpg")  == 0
    ||         strcmp(ext, "JPG")  == 0
    ||         strcmp(ext, "jpeg") == 0
    ||         strcmp(ext, "JPEG") == 0) {

        image = HPDF_LoadJpegImageFromFile(pdf->doc, path);
    } else {
        ERR("pdf: bad image format\n");
    }

    tree_insert(pdf->images, (char*)path, image);

out:;
    return image;
}

static void pdf_new_page(pdf_t *pdf) {
    float width;
    float ratio;

    pdf->cur_page = HPDF_AddPage(pdf->doc);
    HPDF_Page_SetSize(pdf->cur_page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

    width = HPDF_Page_GetWidth(pdf->cur_page);
    ratio = width / ((pdf->pres->w < pdf->pres->h)
                        ? pdf->pres->w
                        : pdf->pres->h);

    HPDF_Page_SetWidth(pdf->cur_page,  pdf->pres->w * ratio);
    HPDF_Page_SetHeight(pdf->cur_page, pdf->pres->h * ratio);
    HPDF_Page_Concat(pdf->cur_page, ratio, 0, 0, ratio, 0, 0);

    HPDF_Page_SetRGBFill(pdf->cur_page, pdf->pres->r / 255.0, pdf->pres->g / 255.0, pdf->pres->b / 255.0);
    HPDF_Page_Rectangle(pdf->cur_page, 0, 0, pdf->pres->w, pdf->pres->h);
    HPDF_Page_ClosePathFillStroke(pdf->cur_page);

    pdf->pres->draw_y = 0;
}

static void pdf_para(pdf_t *pdf, pres_elem_t *elem) {
    pres_t         *pres;
    font_cache_t   *font;
    HPDF_Font       hfont;
    int             underline_line_height;
    int             _x, _y;
    array_t         wrap_points;
    array_t         line_widths;
    int             line;
    int             i;
    int             elem_start_x;
    pres_elem_t    *eit;
    char           *cit;
    char            c[5] = { 0 };
    int             wrapped;
    char_code_t     code;
    int             n_bytes;
    font_entry_t   *entry;
    int            *wrap_it;

    pres = pdf->pres;

    font = pres_get_elem_font(pres, elem);

    underline_line_height = font->line_height;

    _x = pres->draw_x + elem->l_margin;
    _y = pres->draw_y;
    (void)_y;

    pres->draw_x  = _x;
    pres->draw_y += font->line_height;

    /* compute_para_text() */
    wrap_points = elem->wrap_points;
    line_widths = elem->line_widths;

    line = 0;

    switch (elem->justification) {
        case JUST_L: break;
        case JUST_R:
            pres->draw_x += (pres->w - elem->l_margin - elem->r_margin) - *(int*)array_item(line_widths, line);
            break;
        case JUST_C:
            pres->draw_x += ((pres->w - elem->l_margin - elem->r_margin) - *(int*)array_item(line_widths, line)) / 2;
            break;
    }

    i = 0;

    array_traverse(elem->para_elems, eit) {
        font           = pres_get_elem_font(pres, eit);
        pres->cur_font = font;
        hfont          = get_pdf_font(pdf, font->path);

        HPDF_Page_SetFontAndSize(pdf->cur_page, hfont, eit->font_size * 4);
        HPDF_Page_SetRGBFill(pdf->cur_page, eit->r / 255.0, eit->g / 255.0, eit->b / 255.0);

        elem_start_x = pres->draw_x;

        array_traverse(eit->text, cit) {
            wrapped = 0;

            code  = get_char_code(cit, &n_bytes);
            entry = get_glyph(font, code, NULL);

            memcpy(c, cit, n_bytes);
            c[n_bytes] = 0;

            array_traverse(wrap_points, wrap_it) {
                if (*wrap_it == i) {
                    line         += 1;
                    pres->draw_y += 1.25 * font->line_height;
                    pres->draw_x  = _x;

                    switch (elem->justification) {
                        case JUST_L: break;
                        case JUST_R:
                            pres->draw_x += (pres->w - elem->l_margin - elem->r_margin) - *(int*)array_item(line_widths, line);
                            break;
                        case JUST_C:
                            pres->draw_x += ((pres->w - elem->l_margin - elem->r_margin) - *(int*)array_item(line_widths, line)) / 2;
                            break;
                    }

                    elem_start_x = pres->draw_x;

                    wrapped = 1;
                    break;
                }
            }

            HPDF_Page_BeginText(pdf->cur_page);
            HPDF_Page_SetTextRenderingMode(pdf->cur_page, HPDF_FILL);
            HPDF_Page_TextOut(pdf->cur_page, pres->draw_x, pres->h - pres->draw_y, c);
            HPDF_Page_EndText(pdf->cur_page);

            if (eit->flags & PRES_UNDERLINE) {
                HPDF_Page_SetRGBStroke(pdf->cur_page, eit->r / 255.0, eit->g / 255.0, eit->b / 255.0);
                HPDF_Page_SetRGBFill(pdf->cur_page, eit->r / 255.0, eit->g / 255.0, eit->b / 255.0);
                HPDF_Page_Rectangle(pdf->cur_page,
                                    elem_start_x,
                                    pres->h - pres->draw_y - 0.025 * underline_line_height,
                                    pres->draw_x - elem_start_x + entry->pen_advance_x,
                                    0.025 * underline_line_height);
                HPDF_Page_ClosePathFillStroke(pdf->cur_page);
            }

            if (!wrapped) {
                pres->draw_x += entry->pen_advance_x;
            }
            pres->draw_y += entry->pen_advance_y;

            /* Update our iterator if this was a unicode multibyte sequence. */
            cit += n_bytes - 1;
            i   += n_bytes;
        }
    }

/*     HPDF_Page_EndText(pdf->cur_page); */

    pres->is_translating = 0;
}

static void pdf_string(pdf_t *pdf, const char *str, int l_margin, int r_margin, int justification) {
    pres_t         *pres;
    int             _x, _y;
    int             len;
    int             i;
    font_entry_t   *entry;
    array_t         wrap_points;
    array_t         line_widths;
    int            *wrap_it;
    int             wrapped;
    unsigned char  *bytes;
    char_code_t     code;
    int             n_bytes;
    int             line;
    font_cache_t   *font;
    HPDF_Font       hfont;
    char            c[5] = { 0 };

    if (!str) { return; }

    pres = pdf->pres;

    font = pres->cur_font;
    if (!font) { return; }

    HPDF_Page_BeginText(pdf->cur_page);
    HPDF_Page_SetTextRenderingMode(pdf->cur_page, HPDF_FILL);

    hfont = get_pdf_font(pdf, font->path);
    HPDF_Page_SetFontAndSize(pdf->cur_page, hfont, pres->cur_font->size * 4);

    _x = pres->draw_x + l_margin;
    _y = pres->draw_y;

    (void)_y;

    pres->draw_x  = _x;
    pres->draw_y += font->line_height;

    bytes = (unsigned char*)str;

    len = strlen(str);

    wrap_points = get_wrap_points(pres, bytes, l_margin, r_margin, &line_widths);
    line        = 0;

    switch (justification) {
        case JUST_L: break;
        case JUST_R:
            pres->draw_x += (pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line);
            break;
        case JUST_C:
            pres->draw_x += ((pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
            break;
    }

    for (i = 0; i < len;) {
        wrapped = 0;

        code  = get_char_code(str + i, &n_bytes);
        entry = get_glyph(font, code, NULL);

        memcpy(c, str + i, n_bytes);
        c[n_bytes] = 0;

        array_traverse(wrap_points, wrap_it) {
            if (*wrap_it == i) {
                line   += 1;
                pres->draw_y += 1.25 * font->line_height;
                pres->draw_x  = _x;

                switch (justification) {
                    case JUST_L: break;
                    case JUST_R:
                        pres->draw_x += (pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line);
                        break;
                    case JUST_C:
                        pres->draw_x += ((pres->w - l_margin - r_margin) - *(int*)array_item(line_widths, line)) / 2;
                        break;
                }

                wrapped = 1;
                break;
            }
        }

        HPDF_Page_TextOut(pdf->cur_page, pres->draw_x, pres->h - pres->draw_y, c);

        if (!wrapped) {
            pres->draw_x += entry->pen_advance_x;
        }
        pres->draw_y += entry->pen_advance_y;

        i += n_bytes;
    }

    array_free(line_widths);
    array_free(wrap_points);


    HPDF_Page_EndText(pdf->cur_page);
}

static void pdf_bullet(pdf_t *pdf, pres_elem_t *elem) {
    int save_draw_x,
        save_draw_y;
    int new_l_margin;
    int save_l_margin;

    pdf->pres->cur_font = pres_get_elem_font(pdf->pres, elem);

    HPDF_Page_SetRGBFill(pdf->cur_page, elem->r / 255.0, elem->g / 255.0, elem->b / 255.0);

    save_draw_x = pdf->pres->draw_x;
    save_draw_y = pdf->pres->draw_y;

    new_l_margin =   elem->l_margin
                   + ((0.05 * (elem->level - 1)) * pdf->pres->w);

    pdf_string(pdf,
               pdf->pres->bullet_strings[elem->level - 1],
               new_l_margin, elem->r_margin, JUST_L);

    new_l_margin = pdf->pres->draw_x - save_draw_x;
    pdf->pres->draw_x = save_draw_x;
    pdf->pres->draw_y = save_draw_y;

    save_l_margin  = elem->l_margin;
    elem->l_margin = new_l_margin;
    pdf_para(pdf, elem);
    elem->l_margin = save_l_margin;

    pdf->pres->is_translating = 0;
}

static void pdf_break(pdf_t *pdf, pres_elem_t *elem) {
    if (pdf->pres->cur_font) {
        pdf->pres->draw_y += 0.75 * pdf->pres->cur_font->line_height;
    }
    pdf->pres->is_translating = 0;
}

static void pdf_vspace(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->draw_y += elem->y;
    pdf->pres->is_translating = 0;
}

static void pdf_vfill(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->draw_y += pdf->pres->h - (pdf->pres->draw_y % pdf->pres->h);
    pdf->pres->is_translating = 0;
}

static void pdf_image(pdf_t *pdf, pres_elem_t *elem) {
    HPDF_Image image;

    image = get_image(pdf, elem->image);

    HPDF_Page_DrawImage(pdf->cur_page,
                        image,
                        pdf->pres->draw_x, pdf->pres->h - pdf->pres->draw_y - elem->h, elem->w, elem->h);

    pdf->pres->draw_y         += elem->h;
    pdf->pres->is_translating  = 0;
}

static void pdf_save(pdf_t *pdf, pres_elem_t *elem) {
    mark_name_t key;
    mark_map_it it;

    key = elem->mark_name;

    it = tree_lookup(pdf->pres->marks, elem->mark_name);
    if (!tree_it_good(it)) { key = strdup(key); }

    tree_insert(pdf->pres->marks, key, pdf->pres->draw_y);
}

static void pdf_restore(pdf_t *pdf, pres_elem_t *elem) {
    mark_map_it it;
    int         dst;

    dst = 0;
    it  = tree_lookup(pdf->pres->marks, elem->mark_name);
    if (tree_it_good(it)) {
        dst = tree_it_val(it);
    }

    pdf->pres->draw_y = dst;
}

static void pdf_goto(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->is_translating = 1;
    pdf->pres->draw_x = elem->x;
    pdf->pres->draw_y = pdf->pres->draw_y - (pdf->pres->draw_y % pdf->pres->h) + elem->y;
}

static void pdf_gotox(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->is_translating = 1;
    pdf->pres->draw_x = pdf->pres->view_x + elem->x;
}

static void pdf_gotoy(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->draw_y = pdf->pres->draw_y - (pdf->pres->draw_y % pdf->pres->h) + elem->y;
}

static void pdf_translate(pdf_t *pdf, pres_elem_t *elem) {
    pdf->pres->is_translating  = 1;
    pdf->pres->draw_x         += elem->x;
    pdf->pres->draw_y         += elem->y;
}

void export_to_pdf(pres_t *pres, const char *path) {
    pdf_t        pdf;
    pres_elem_t *elem;

    pdf.pres   = pres;
    pdf.fonts  = tree_make_c(font_name_t, HPDF_Font, strcmp);
    pdf.images = tree_make_c(image_path_t, HPDF_Image, strcmp);

    pdf.doc = HPDF_New (error_handler, NULL);
    if (!pdf.doc) {
        ERR("pdf: cannot create PDF doc object\n");
    }

    HPDF_SetCompressionMode(pdf.doc, HPDF_COMP_ALL);
    HPDF_SetPageMode(pdf.doc, HPDF_PAGE_MODE_USE_THUMBS);
    HPDF_UseUTFEncodings(pdf.doc);

    pdf_new_page(&pdf);

    array_traverse(pres->elements, elem) {
        switch (elem->kind) {
            case PRES_PARA:      pdf_para(&pdf, elem);      break;
            case PRES_BULLET:    pdf_bullet(&pdf, elem);    break;
            case PRES_BREAK:     pdf_break(&pdf, elem);     break;
            case PRES_VSPACE:    pdf_vspace(&pdf, elem);    break;
            case PRES_VFILL:     pdf_vfill(&pdf, elem);     break;
            case PRES_IMAGE:     pdf_image(&pdf, elem);     break;
            case PRES_SAVE:      pdf_save(&pdf, elem);      break;
            case PRES_RESTORE:   pdf_restore(&pdf, elem);   break;
            case PRES_GOTO:      pdf_goto(&pdf, elem);      break;
            case PRES_GOTOX:     pdf_gotox(&pdf, elem);     break;
            case PRES_GOTOY:     pdf_gotoy(&pdf, elem);     break;
            case PRES_TRANSLATE: pdf_translate(&pdf, elem); break;
            case PRES_POINT:
                if (pres->draw_y >= pres->h) { pdf_new_page(&pdf); }
        }

        if (!pres->is_translating) { pres->draw_x = 0; }
    }

    HPDF_SaveToFile(pdf.doc, path);
    HPDF_Free(pdf.doc);
}
