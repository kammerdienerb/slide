#include "pdf.h"
#include "tree.h"

use_tree(int, int);

typedef tree(int, int) object_offset_tree;

typedef struct {
    pres_t             *pres;
    SDL_Renderer       *sdl_ren;
    FILE               *f;
    const char         *path;
    float               quality;
    int                 w;
    int                 h;
    int                 n_pages;
    object_offset_tree  offsets;
    int                 xref_offset;
    void               *pix_buff;
} pdf_t;

static void count_pages(pdf_t *pdf) {
    pres_elem_t *elem;

    pdf->n_pages = 0;
    array_traverse(pdf->pres->elements, elem) {
        if (elem->kind == PRES_POINT) {
            pdf->n_pages += 1;
        }
    }
}

static int fsize(FILE *f) {
    int save;
    int size;

    fflush(f);

    save = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, save, SEEK_SET);

    return size;
}

static void begin_obj(pdf_t *pdf, int obj_id) {
    int file_offset;

    file_offset = fsize(pdf->f);

    tree_insert(pdf->offsets, obj_id, file_offset);
}

static void write_pdf_header(pdf_t *pdf) {
    int i;

    fprintf(pdf->f,
"%%PDF-1.1\n");
    fprintf(pdf->f,
"%%¥±ë\n\n");

    /*
     * Create the catalog.
     * The pages dictionary is object 2.
     */
    begin_obj(pdf, 1);
    fprintf(pdf->f,
"1 0 obj\n"
"  << /Type /Catalog\n"
"     /Pages 2 0 R\n"
"  >>\n"
"endobj\n\n");

    /*
     * Create the pages dictionary.
     * Specify the number of pages and the size of a page.
     */
    begin_obj(pdf, 2);
    fprintf(pdf->f,
"2 0 obj\n"
"  << /Type /Pages\n"
"     /Kids [\n");
    for (i = 0; i < pdf->n_pages; i += 1) {
        fprintf(pdf->f,
"       %d 0 R\n",
        3 + (3 * i));
    }

    fprintf(pdf->f,
"     ]\n");

    fprintf(pdf->f,
"     /Count %d\n"
"     /MediaBox [0 0 %d %d]\n"
"  >>\n"
"endobj\n\n",
    pdf->n_pages,
    pdf->w,
    pdf->h);
}


static void write_pdf_page(pdf_t *pdf, int p) {
    char         Do_buff[512];
    int          len;
    int          i;
    SDL_Surface *surface1,
                *surface2;
    int          num_scaled_pixels;

    /* +3 is used for objects defined in header. */

    /* Write the page object. */
    begin_obj(pdf, 3 + (3 * p));
    fprintf(pdf->f,
"%d 0 obj\n"
"  <<  /Type /Page\n"
"      /Parent 2 0 R\n"
"      /Contents %d 0 R\n"
"      /Resources\n"
"        << /XObject\n"
"          << /Im%d %d 0 R >>\n"
"        >>\n"
"  >>\n"
"endobj\n\n",
    3 + (3 * p),      /* This object                 */
    3 + (3 * p) + 1,  /* The Do stream object number */
    p,                /* The image number            */
    3 + (3 * p) + 2); /* The image XObject number    */

    /* Write the Do stream object. */
    sprintf(Do_buff,
"q\n"
"%d 0 0 %d 0 0 cm\n"
"/Im%d Do\n"
"Q",
    pdf->w,
    pdf->h,
    p);

    len = strlen(Do_buff);

    begin_obj(pdf, 3 + (3 * p) + 1);
    fprintf(pdf->f,
"%d 0 obj\n"
"  << /Length %d >>\n"
"stream\n"
"%s\n"
"endstream\n"
"endobj\n\n",
    3 + (3 * p) + 1,
    len,
    Do_buff);



    surface1 = SDL_CreateRGBSurfaceWithFormat(
                    0,
                    pdf->pres->w,
                    pdf->pres->h,
                    32,
                    SDL_PIXELFORMAT_ARGB8888);

    SDL_RenderReadPixels(pdf->sdl_ren,
                         NULL,
                         SDL_PIXELFORMAT_ARGB8888,
                         surface1->pixels,
                         4 * pdf->pres->w);

    if (pdf->quality < 1.0) {
        surface2 = SDL_CreateRGBSurfaceWithFormat(
                        0,
                        pdf->w,
                        pdf->h,
                        32,
                        SDL_PIXELFORMAT_ARGB8888);

        SDL_BlitScaled(surface1, NULL, surface2, NULL);
    } else {
        surface2 = surface1;
    }

    num_scaled_pixels = pdf->w * pdf->h;

    for (i = 0; i < num_scaled_pixels; i += 1) {
        ((char*)pdf->pix_buff)[3 * i + 0] = ((char*)surface2->pixels)[4 * i + 2];
        ((char*)pdf->pix_buff)[3 * i + 1] = ((char*)surface2->pixels)[4 * i + 1];
        ((char*)pdf->pix_buff)[3 * i + 2] = ((char*)surface2->pixels)[4 * i + 0];
    }


    begin_obj(pdf, 3 + (3 * p) + 2);
    fprintf(pdf->f,
"%d 0 obj\n"
"  << /Type /XObject\n"
"     /Subtype /Image\n"
"     /BitsPerComponent 8\n"
"     /ColorSpace /DeviceRGB\n"
"     /Width %d\n"
"     /Height %d\n"
"     /Length %d\n"
"  >>\n"
"stream\n",
        3 + (3 * p) + 2,
        pdf->w,
        pdf->h,
        3 * num_scaled_pixels);

    fwrite(pdf->pix_buff, 3, num_scaled_pixels, pdf->f);

    fprintf(pdf->f,
"\nendstream\n"
"endobj\n");

    SDL_FreeSurface(surface2);
    if (surface1 != surface2) {
        SDL_FreeSurface(surface1);
    }
}

static void write_pdf_xref(pdf_t *pdf) {
    int               n_objects;
    tree_it(int, int) it;

    pdf->xref_offset = fsize(pdf->f);

    fprintf(pdf->f,
"xref\n");

    n_objects = tree_len(pdf->offsets) + 1;

    fprintf(pdf->f,
"0 %d\n",
    n_objects);

    fprintf(pdf->f,
"0000000000 65535 f \n");

    tree_traverse(pdf->offsets, it) {
        fprintf(pdf->f,
"%010d 00000 n \n",
            tree_it_val(it));
    }
}

static void write_pdf_trailer(pdf_t *pdf) {
    int n_objects;

    n_objects = tree_len(pdf->offsets) + 1;

    fprintf(pdf->f,
"trailer\n"
"  << /Root 1 0 R\n"
"     /Size %d\n"
"  >>\n", n_objects);

    fprintf(pdf->f,
"startxref\n");
    fprintf(pdf->f,
"%d\n", pdf->xref_offset);

    fprintf(pdf->f,
"%%%%EOF");
}

void export_to_pdf(SDL_Renderer *sdl_ren, pres_t *pres, const char *path, float quality) {
    pdf_t  pdf;
    double pres_speed;
    int    p;

    /* Save so that it can be restored later. */
    pres_speed  = pres->speed;
    pres->speed = INFINITY;

    pdf.pres    = pres;
    pdf.sdl_ren = sdl_ren;
    pdf.f       = fopen(path, "w");
    if (!pdf.f) {
        ERR("could not fopen '%s' for writing\n", path);
    }
    pdf.path      = path;
    pdf.quality   = quality;
    pdf.w         = quality * pres->w;
    pdf.h         = quality * pres->h;
    pdf.offsets   = tree_make(int, int);
    pdf.pix_buff  = malloc(3 * pdf.w * pdf.h);

    SDL_RenderClear(sdl_ren);

    count_pages(&pdf);
    write_pdf_header(&pdf);
    for (p = 0; p <= pdf.n_pages; p += 1) {
        draw_presentation(pres);
        SDL_RenderFlush(sdl_ren);
        write_pdf_page(&pdf, p);
        SDL_RenderPresent(sdl_ren);
        SDL_Delay(0);
        pres_next_point(pres);
        update_presentation(pres);
    }
    write_pdf_xref(&pdf);
    write_pdf_trailer(&pdf);

    free(pdf.pix_buff);
    fclose(pdf.f);
    tree_free(pdf.offsets);

    pres->speed = pres_speed;
}
