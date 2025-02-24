/* SPDX-License-Identifier: Zlib */

#include "adjustment.h"
#include "utils.h"
#include "page.h"
#include <girara/utils.h>

#include <math.h>

double page_calc_height_width(zathura_document_t* document, double height, double width, unsigned int* page_height,
                              unsigned int* page_width, bool rotate) {
  g_return_val_if_fail(document != NULL && page_height != NULL && page_width != NULL, 0.0);

  double scale = zathura_document_get_scale(document);

  if (rotate == true && zathura_document_get_rotation(document) % 180 != 0) {
    *page_width  = round(height * scale);
    *page_height = round(width * scale);
    scale        = MAX(*page_width / height, *page_height / width);
  } else {
    *page_width  = round(width * scale);
    *page_height = round(height * scale);
    scale        = MAX(*page_width / width, *page_height / height);
  }

  return scale;
}

void page_calc_position(zathura_document_t* document, double x, double y, double* xn, double* yn) {
  g_return_if_fail(document != NULL && xn != NULL && yn != NULL);

  const unsigned int rot = zathura_document_get_rotation(document);
  if (rot == 90) {
    *xn = 1 - y;
    *yn = x;
  } else if (rot == 180) {
    *xn = 1 - x;
    *yn = 1 - y;
  } else if (rot == 270) {
    *xn = y;
    *yn = 1 - x;
  } else {
    *xn = x;
    *yn = y;
  }
}

unsigned int position_to_page_number(zathura_document_t* document, double pos_x, double pos_y) {
  g_return_val_if_fail(document != NULL, 0);

  unsigned int document_height = 0, document_width = 0;
  zathura_document_get_document_size(document, &document_height, &document_width);
  g_return_val_if_fail(document_height != 0 && document_width != 0, 0);

  const unsigned int npag = zathura_document_get_number_of_pages(document);
  const unsigned int ncol = zathura_document_get_pages_per_row(document);
  g_return_val_if_fail(npag != 0 && ncol != 0, 0);
  const unsigned int c0 = zathura_document_get_first_page_column(document);

  unsigned int page_row = 0, page_col = 0;

  /* find row */
  const unsigned int nrow = ceil((double)(npag + c0 - 1) / ncol);
  for (unsigned int row = 1; row <= nrow; row++) {
    unsigned int first_page_id, last_page_id;
    get_row_range(row, c0, ncol, npag, &first_page_id, &last_page_id);
    unsigned int y = 0, dontcare = 0;
    zathura_document_get_page_tail_position(document, first_page_id, &dontcare, &y);
    if ((double)y / document_height >= pos_y) {
      page_row = row;
      break;
    }
  }
  if (page_row == 0) {
    page_row = nrow;
  }

  /* find column */
  for (unsigned int col = 1; col <= ncol; col++) {
    unsigned int first_page_id, last_page_id;
    get_column_range(col, c0, ncol, npag, &first_page_id, &last_page_id);
    unsigned int x = 0, dontcare = 0;
    zathura_document_get_page_tail_position(document, first_page_id, &x, &dontcare);
    if ((double)x / document_width >= pos_x) {
      page_col = col;
      break;
    }
  }
  if (page_col == 0) {
    page_col = ncol;
  }

  int pn = (page_row - 1) * ncol + page_col - c0;
  if (pn < 0) {
    pn = 0;
  }
  if (pn > (int)npag - 1) {
    pn = npag - 1;
  }
  return pn;
}

void page_number_to_position(zathura_document_t* document, unsigned int page_number, double xalign, double yalign,
                             double* pos_x, double* pos_y) {
  zathura_page_t* page = zathura_document_get_page(document, page_number);
  g_return_if_fail(document != NULL && page != NULL);
  unsigned int page_height = 0;
  unsigned int page_width  = 0;
  const double height      = zathura_page_get_height(page);
  const double width       = zathura_page_get_width(page);
  page_calc_height_width(document, height, width, &page_height, &page_width, true);

  unsigned int document_height = 0, document_width = 0;
  zathura_document_get_document_size(document, &document_height, &document_width);
  g_return_if_fail(document_height != 0 && document_width != 0);

  unsigned int view_height = 0, view_width = 0;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  unsigned int x = 0, y = 0;

  if (page_number > 0) {
    zathura_document_get_page_tail_position(document, page_number - 1, &x, &y);
  }

  /* compute the shift to align to the viewport. If the page fits to viewport, just center it. */

  y += page_height *
       (page_height > view_height ? (0.5 + (yalign - 0.5) * (page_height - view_height) / page_height) : 0.5);
  x += page_width * (page_width > view_width ? 0.5 + (xalign - 0.5) * (page_width - view_width) / page_width : 0.5);

  *pos_y = (double)y / document_height;
  *pos_x = (double)x / document_width;
}

bool page_is_visible(zathura_document_t* document, unsigned int page_number) {
  zathura_page_t* page = zathura_document_get_page(document, page_number);
  g_return_val_if_fail(document != NULL && page != NULL, false);
  unsigned int page_height = 0;
  unsigned int page_width  = 0;
  const double height      = zathura_page_get_height(page);
  const double width       = zathura_page_get_width(page);
  page_calc_height_width(document, height, width, &page_height, &page_width, true);

  /* position at the center of the viewport */
  double pos_x = zathura_document_get_position_x(document);
  double pos_y = zathura_document_get_position_y(document);

  /* get the center of page page_number */
  double page_x, page_y;
  page_number_to_position(document, page_number, 0.5, 0.5, &page_x, &page_y);

  unsigned int document_height, document_width;
  zathura_document_get_document_size(document, &document_height, &document_width);

  unsigned int view_width, view_height;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  return (fabs(pos_x - page_x) < 0.5 * (double)(view_width + page_width) / (double)document_width &&
          fabs(pos_y - page_y) < 0.5 * (double)(view_height + page_height) / (double)document_height);
}

void zathura_adjustment_set_value(GtkAdjustment* adjustment, gdouble value) {
  const gdouble lower        = gtk_adjustment_get_lower(adjustment);
  const gdouble upper_m_size = gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);

  gtk_adjustment_set_value(adjustment, MAX(lower, MIN(upper_m_size, value)));
}

gdouble zathura_adjustment_get_ratio(GtkAdjustment* adjustment) {
  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);
  gdouble value     = gtk_adjustment_get_value(adjustment);

  return (value - lower + page_size / 2.0) / (upper - lower);
}

void zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment, gdouble ratio) {
  if (ratio == 0.0) {
    return;
  }

  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);

  gdouble value = (upper - lower) * ratio + lower - page_size / 2.0;

  zathura_adjustment_set_value(adjustment, value);
}
