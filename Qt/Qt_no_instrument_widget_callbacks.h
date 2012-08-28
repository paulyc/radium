#if USE_QT4
//#include <QCleanlooksStyle>
//#include <QOxygenStyle>
#include <QPlastiqueStyle>
#endif

#include "../common/patch_proc.h"

#include "Qt_no_instrument_widget.h"

extern "C" void set_font_thickness(int val);
extern QString default_style_name;

class No_instrument_widget : public QWidget, public Ui::No_instrument_widget{
  Q_OBJECT

 public:
  bool initing;

 No_instrument_widget(QWidget *parent=NULL)
    : QWidget(parent)
  {
    initing = true;
    setupUi(this);
    use_system_colors->setChecked(!SETTINGS_read_bool("override_default_qt_colors",true));
    use_system_style->setChecked(!SETTINGS_read_bool("override_default_qt_style",true));
    initing = false;
  }

public slots:
  void on_create_instrument_clicked()
  {
    printf("Got it\n");
    if(initing==true)
      return;

    struct Patch *patch = NewPatchCurrPos();
    addInstrument(patch);
    Instrument_widget *instrument = get_instrument_widget(patch);
    instruments_widget->tabs->showPage(instrument);
  }

  void on_use_system_colors_stateChanged(int state){
    if(initing==true)
      return;

    bool override;

    if(state==Qt::Unchecked)
      override = true;
    else if(state==Qt::Checked)
      override = false;
    else
      return;

    SETTINGS_write_bool("override_default_qt_colors",override);

    setApplicationColors(qapplication);
  }

  void on_use_system_style_stateChanged(int state){
    if(initing==true)
      return;

    bool override;

    if(state==Qt::Unchecked)
      override = true;
    else if(state==Qt::Checked)
      override = false;
    else
      return;

    SETTINGS_write_bool("override_default_qt_style",override);

    printf("default: %s\n",default_style_name.ascii());

    if(override==true)
      QApplication::setStyle("plastique");
    else
      QApplication::setStyle(default_style_name);
  }

  void on_midi_input_stateChanged(int state){
    if(initing==true)
      return;

    if(state==Qt::Unchecked)
      root->editonoff = false;
    else if(state==Qt::Checked)
      root->editonoff = true;
    else
      return;

    struct Tracker_Windows *window = root->song->tracker_windows;
    char temp[1000];
    sprintf(temp,"Midi Input %s",root->editonoff?"On":"Off");
    GFX_SetStatusBar(window,temp);
  }

  /*
  void on_font_thickness_slider_valueChanged( int val){
    set_font_thickness(val);
  }
  */
};
