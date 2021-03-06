
#ifndef _RADIUM_QT_QT_SEQUENCER_PROC_H
#define _RADIUM_QT_QT_SEQUENCER_PROC_H

void SEQUENCER_WIDGET_initialize(QWidget *main_window);

void SEQUENCER_WIDGET_call_very_often(void);

void SEQUENCER_zoom_or_move_leftright(bool do_zoom, int inc, double middle_pos = 0.5); // middle_pos is only relevant if do_zoom is true.
  
QWidget *SEQUENCER_WIDGET_get_widget(void);

class QPainter;

// returns true if all text was painted
bool myDrawText(QPainter &p, QRectF rect, QString text, int flags = Qt::AlignLeft | Qt::AlignTop, bool wrap_lines = false, int rotate = 0, bool scale_font_size = false, bool cut_text_to_fit = false);

#endif
