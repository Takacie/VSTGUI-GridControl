//----------------------------------------------------------------------------------------------------------------------
// Copyright(c) 2021 iNVOX Records Takacie.
//----------------------------------------------------------------------------------------------------------------------
#ifndef IR_GRIDCONTROL_H
#define IR_GRIDCONTROL_H

#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cdrawcontext.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "base/source/fstreamer.h"

using namespace VSTGUI;
using namespace Steinberg;

namespace INVOXRecords {
//----------------------------------------------------------------------------------------------------------------------
//  alias
//----------------------------------------------------------------------------------------------------------------------
using BarVector = std::vector<float>;
using BarVectorPtr = std::unique_ptr<BarVector>;

//----------------------------------------------------------------------------------------------------------------------
//  constants
//----------------------------------------------------------------------------------------------------------------------
constexpr int INITIAL_NUM_GRID_BAR = 4;
constexpr int MAX_NUM_GRID_BAR = 16;
constexpr int MIN_NUM_GRID_BAR = 1;
constexpr int INITIAL_NUM_GRID_SPLIT = 2;
constexpr int MAX_NUM_GRID_SPLIT = 8;
constexpr int MIN_NUM_GRID_SPLIT = 1;
constexpr int SNAP_RANGE = 15; // pixel
constexpr float VALUE_MIX = 0.75f;

//----------------------------------------------------------------------------------------------------------------------
//  GridControlButton class
//----------------------------------------------------------------------------------------------------------------------
template<class T>
class GridControlButton : public CTextButton
{
public:
  // constructor
  GridControlButton<T>() = delete;
  GridControlButton<T>(const CRect size, IControlListener* editor, UTF8StringPtr text, T* target, std::function<void(T*)> function);

  // destructor
  ~GridControlButton() = default;

  // common method
  CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons) override;

private:
  T* target;
  std::function<void(T*)> func;
};

template<class T>
inline GridControlButton<T>::GridControlButton(const CRect size, IControlListener* editor, UTF8StringPtr text, T* target, std::function<void(T*)> function) :
  CTextButton(size, editor, -1, text),
  target(target),
  func(function)
{}

template<class T>
inline CMouseEventResult GridControlButton<T>::onMouseUp(CPoint& where, const CButtonState& buttons)
{
  if (isEditing()) {
    if (func) func(target);
  }
  return CTextButton::onMouseUp(where, buttons);
}

//----------------------------------------------------------------------------------------------------------------------
//  GridControlController class
//----------------------------------------------------------------------------------------------------------------------
class GridControlController : public CControl
{
public:
  // constructor
  GridControlController(IControlListener* listener, int tag, float min_value = 0.0f, float max_value = 1.0f);

  // destructor
  ~GridControlController() = default;

  // common method
  void TimerProcess();

  void draw(CDrawContext* pContext) override {}
  CBaseObject* newCopy() const override { return nullptr; }

  // getter
  BarVectorPtr& getBarVectorPtr() { return bar_vector; }
  int* getBarNumPtr() { return &bar_num; }
  int getBarNum() { return bar_num; }
  int getVectorSize() { return bar_vector.get()->size(); }
  Vst::TQuarterNotes getProjectTimeMusic() { return *project_time_music; }
  int getCurrentIndex() { return current_index; }
  float getMaxValue() { return max_value; }
  float getMinValue() { return min_value; }

  // setter
  void setProjectTimeMusic(Vst::TQuarterNotes* value) { project_time_music = value; }
  void setBarNum(int value) { bar_num = value; }

  // use in processor thread onTimer()
  static void sendHostInfoMessage(Vst::AudioEffect* processor, Vst::TQuarterNotes project_time_music);
  // use in controller thread notify()
  static tresult notifyHostInfoMessage(GridControlController* grid_control, Vst::IMessage* message);
  // use in controller thread setState()
  static tresult setControlState(GridControlController* grid_control, IBStreamer streamer);
  // use in controller thread getState()
  static void getControlState(GridControlController* grid_control, IBStreamer streamer);

private:
  int bar_num = INITIAL_NUM_GRID_BAR;
  int current_index = 0;
  BarVectorPtr bar_vector;

  Vst::TQuarterNotes* project_time_music = 0;

  float min_value;
  float max_value;
};

inline GridControlController::GridControlController(IControlListener* listener, int tag, float min_value, float max_value) :
  CControl(CRect(), listener, tag),
  bar_vector(std::make_unique<BarVector>(BarVector{})),
  min_value(min_value),
  max_value(max_value)
{
  for (int i = 0; i < INITIAL_NUM_GRID_BAR; i++) {
    bar_vector.get()->push_back(0.5f);
  }
}

inline void GridControlController::TimerProcess()
{
  const int fixed_project_time_music = static_cast<int>(std::round(getProjectTimeMusic() * 100)) % 400; // 0 ~ 399
  current_index = static_cast<int>(fixed_project_time_music / (400 / bar_num));
  float old_value = getValue();
  float new_value = (max_value - min_value) * (bar_vector.get()->at(current_index));
  beginEdit();
  setValue(VALUE_MIX * new_value + (1.0f - VALUE_MIX) * old_value);
  valueChanged();
  endEdit();
}

inline void GridControlController::sendHostInfoMessage(Vst::AudioEffect* processor, Vst::TQuarterNotes project_time_music)
{
  Vst::IMessage* GridControlMsg = processor->allocateMessage();
  GridControlMsg->setMessageID(u8"GridControlMsg");
  GridControlMsg->getAttributes()->setBinary(u8"project_time_music", (void*)&project_time_music, sizeof(Vst::TQuarterNotes));
  processor->sendMessage(GridControlMsg);
  GridControlMsg->release();
}

inline tresult GridControlController::notifyHostInfoMessage(GridControlController* grid_control, Vst::IMessage* message)
{
  if (grid_control && message) {
    if (!strcmp(message->getMessageID(), u8"GridControlMsg")) {
      const void* data;
      uint32 size;
      message->getAttributes()->getBinary(u8"project_time_music", data, size);
      if (grid_control) {
        grid_control->setProjectTimeMusic((Vst::TQuarterNotes*)data);
      }
      return kResultOk;
    }
  }
}

inline tresult GridControlController::setControlState(GridControlController* gc_control, IBStreamer streamer)
{
  tresult result = kResultOk;

  int16 bar_num;
  if (!streamer.readInt16(bar_num)) result = kResultFalse;
  gc_control->setBarNum(bar_num);

  BarVector vector;
  for (int i = 0; i < bar_num; i++) {
    float value;
    if (!streamer.readFloat(value)) result = kResultFalse;
    vector.push_back(value);
  }
  *gc_control->bar_vector.get() = std::move(vector);
  return result;
}

inline void GridControlController::getControlState(GridControlController* gc_control, IBStreamer streamer)
{
  streamer.writeInt16(static_cast<int16>(gc_control->getBarNum()));
  for (int i = 0; i < gc_control->getBarNum(); i++) {
    streamer.writeFloat(gc_control->bar_vector.get()->at(i));
  }
}

//----------------------------------------------------------------------------------------------------------------------
//  GridControl class
//----------------------------------------------------------------------------------------------------------------------
class GridControl : public CControl
{
public:
  // constructor
  GridControl(IControlListener* editor, const CRect size, int tag, float min_value, float max_value, GridControlController* gc_controller);

  // destructor
  ~GridControl();

  // override mothod
  void draw(CDrawContext* pContext) override;
  CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons) override;
  CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons) override;
  CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons) override;
  CMouseEventResult onMouseCancel() override;
  CBaseObject* newCopy() const override { return nullptr; }

  // common method
  void addControlsToFrame(CFrame* frame);

  // getter

  // setter
  void setColors(CColor value1, CColor value2) { left_color = value1; right_color = value2; }

private:
  int split_num = INITIAL_NUM_GRID_SPLIT;

  GridControlController* gc_controller;

  CRect bar_control_rect;
  CViewContainer* control_grid_x;
  GridControlButton<GridControl>* double_grid_x;
  GridControlButton<GridControl>* half_grid_x;
  CViewContainer* control_grid_y;
  GridControlButton<GridControl>* double_grid_y;
  GridControlButton<GridControl>* half_grid_y;
  GridControlButton<GridControl>* text_buttons[4] = { double_grid_x, half_grid_x, double_grid_y, half_grid_y };

  CColor left_color = CColor(66, 245, 66);
  CColor right_color = CColor(66, 245, 200);

  void editBarByMouseEvent(CPoint& where);
  void setTextButtonDetail();
  void drawBackground(CDrawContext* pContext, CColor color);
  void drawVerticalGridLine(CDrawContext* pContext);
  void drawHorizontalGridLine(CDrawContext* pContext);
  void drawBar(CDrawContext* pContext);
  void fixVectorSize();

  // button functions
  static void doubleBarNum(GridControl* gc);
  static void halfBarNum(GridControl* gc);
  static void doubleSplit(GridControl* gc);
  static void halfSplit(GridControl* gc);
};

inline GridControl::GridControl(IControlListener* editor, const CRect size, int tag, float min_value, float max_value, GridControlController* gc_controller) :
  CControl(size, editor, tag),
  gc_controller(gc_controller),
  bar_control_rect(size.left + 20, size.top + 20, size.right, size.bottom),
  control_grid_x(new CViewContainer(CRect(size.left + 20, size.top, size.right, size.top + 20))),
  double_grid_x(new GridControlButton<GridControl>(CRect(control_grid_x->getViewSize().getWidth() / 2, 0, control_grid_x->getViewSize().getWidth(), 20),
                                           editor, u8"＋", this, doubleBarNum)),
  half_grid_x(new GridControlButton<GridControl>(CRect(0, 0, control_grid_x->getViewSize().getWidth() / 2, 20),
                                         editor, u8"ー", this, halfBarNum)),
  control_grid_y(new CViewContainer(CRect(size.left, size.top + 20, size.left + 20, size.bottom))),
  double_grid_y(new GridControlButton<GridControl>(CRect(0, 0, 20, control_grid_y->getViewSize().getHeight() / 2),
                                           editor, u8"＋", this, doubleSplit)),
  half_grid_y(new GridControlButton<GridControl>(CRect(0, control_grid_y->getViewSize().getHeight() / 2, 20, control_grid_y->getViewSize().getHeight()),
                                         editor, u8"ー", this, halfSplit))
{
  control_grid_x->addView(double_grid_x);
  control_grid_x->addView(half_grid_x);
  control_grid_y->addView(double_grid_y);
  control_grid_y->addView(half_grid_y);
  setTextButtonDetail();
}

inline GridControl::~GridControl() {
  for (GridControlButton<GridControl>* button : text_buttons) {
    button->forget();
  }
  control_grid_x->forget();
  control_grid_y->forget();
}

inline void GridControl::draw(CDrawContext* pContext)
{
  drawBackground(pContext, CColor(30, 30, 30));
  fixVectorSize();
  drawBar(pContext);
  drawVerticalGridLine(pContext);
  drawHorizontalGridLine(pContext);
  invalid();
  setDirty();
}

inline CMouseEventResult GridControl::onMouseDown(CPoint& where, const CButtonState& buttons)
{
  if (!(buttons & kLButton)) {
    return kMouseEventNotHandled;
  }
  beginEdit();
  editBarByMouseEvent(where);
  return kMouseEventHandled;
}

inline CMouseEventResult GridControl::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
  if (isEditing()) {
    editBarByMouseEvent(where);
  }
  return kMouseEventHandled;
}

inline CMouseEventResult GridControl::onMouseUp(CPoint& where, const CButtonState& buttons)
{
  if (isEditing()) {
    endEdit();
  }
  return kMouseEventHandled;
}

inline CMouseEventResult GridControl::onMouseCancel()
{
  if (isEditing()) {
    endEdit();
  }
  return kMouseEventHandled;
}

inline void GridControl::addControlsToFrame(CFrame* frame)
{
  frame->addView(this);
  frame->addView(control_grid_x);
  frame->addView(control_grid_y);
}

inline void GridControl::editBarByMouseEvent(CPoint& where)
{
  if (bar_control_rect.pointInside(where)) {
    const int bar_num = gc_controller->getBarNum();
    const int index = static_cast<int>((where.x - bar_control_rect.left) / (bar_control_rect.getWidth() / bar_num));
    int bar_height = bar_control_rect.bottom - where.y;
    // calc snap
    const int snap_height = bar_control_rect.getHeight() / split_num;
    const int SNAP_RANGE = 10;
    if ((bar_height % snap_height) < SNAP_RANGE) {
      bar_height -= (bar_height % snap_height);
    }
    else if (((bar_height + SNAP_RANGE) % snap_height) < SNAP_RANGE) {
      bar_height += SNAP_RANGE - ((bar_height + SNAP_RANGE) % snap_height);
    }

    (*gc_controller->getBarVectorPtr().get())[index] = bar_height / bar_control_rect.getHeight();
    invalid();
  }
}

inline void GridControl::setTextButtonDetail()
{
  for (auto&& text_button : text_buttons) {
    text_button->setRoundRadius(0);
    text_button->setFrameWidth(1);
    text_button->setFrameColor(CColor(70, 70, 70));
    text_button->setFrameColorHighlighted (CColor(70, 70, 70));
    text_button->setGradient(CGradient::create(0.0, 1.0, CColor(60, 60, 60), CColor(60, 60, 60)));
    text_button->setGradientHighlighted(CGradient::create(0.0, 1.0, CColor(50, 50, 50), CColor(50, 50, 50)));
    text_button->setTextColor(CColor(200, 200, 200));
    text_button->getFont()->setSize(18);
  }
}

inline void GridControl::drawBackground(CDrawContext* pContext, CColor color)
{
  pContext->setFillColor(CColor(50, 50, 50));
  pContext->drawRect(bar_control_rect, CDrawStyle::kDrawFilled);
}

inline void GridControl::drawVerticalGridLine(CDrawContext* pContext)
{
  pContext->setLineWidth(2);
  int num_grid_bar = gc_controller->getBarNum();
  float bar_width = bar_control_rect.getWidth() / num_grid_bar;
  CPoint line_begin = bar_control_rect.getTopLeft();
  for (int i = 1; i < num_grid_bar; i++) {
    line_begin.offset(CPoint(bar_width, 0));
    pContext->drawLine(line_begin, CPoint(line_begin.x, line_begin.y + bar_control_rect.getHeight()));
  }
}

inline void GridControl::drawHorizontalGridLine(CDrawContext* pContext)
{
  pContext->setLineWidth(1);
  float split_height = bar_control_rect.getHeight() / split_num;
  CPoint line_begin = bar_control_rect.getTopLeft();
  for (int i = 1; i < split_num; i++) {
    line_begin.offset(CPoint(0, split_height));
    pContext->drawLine(line_begin, CPoint(line_begin.x + bar_control_rect.getWidth(), line_begin.y));
  }
}

inline void GridControl::drawBar(CDrawContext* pContext)
{
  const int bar_num = gc_controller->getBarNum();
  const int current_index = gc_controller->getCurrentIndex();
  const float bar_width = bar_control_rect.getWidth() / bar_num;
  float red_diff = (right_color.red - left_color.red) / bar_num;
  float green_diff = (right_color.green - left_color.green) / bar_num;
  float blue_diff = (right_color.blue - left_color.blue) / bar_num;
  std::function<float(float)> fix = [&](float x) { return (x < 0) ? 0 : x; };
  CColor color(left_color);
  for (int i = 0; i < gc_controller->getBarNum(); i++) {
    pContext->setFillColor(color);
    if (i == current_index) {
      int reduce = 100;
      CColor focus_color(CColor(fix(color.red - reduce), fix(color.green - reduce), fix(color.blue - reduce)));
      pContext->setFillColor(focus_color);
    }
    float bar_height = bar_control_rect.getHeight() * gc_controller->getBarVectorPtr().get()->at(i);
    float x = bar_control_rect.left + bar_width * i;
    CRect bar(x, bar_control_rect.bottom - bar_height, x + bar_width, bar_control_rect.bottom);
    pContext->drawRect(bar, CDrawStyle::kDrawFilled);
    color.red += red_diff;
    color.green += green_diff;
    color.blue += blue_diff;
  }
}

inline void GridControl::fixVectorSize()
{
  const int bar_num = gc_controller->getBarNum();
  const int vector_size = gc_controller->getVectorSize();
  if (vector_size > bar_num) {
    for (int i = 0; i < vector_size - bar_num; i++) {
      gc_controller->getBarVectorPtr().get()->pop_back();
    }
  }
  else if (vector_size < bar_num) {
    for (int i = 0; i < bar_num - vector_size; i++) {
      gc_controller->getBarVectorPtr().get()->push_back(0.5f);
    }
  }
}

inline void GridControl::doubleBarNum(GridControl* gc)
{
  if (gc->gc_controller->getBarNum() != MAX_NUM_GRID_BAR) {
    *gc->gc_controller->getBarNumPtr() *= 2;
  }
}

inline void GridControl::halfBarNum(GridControl* gc)
{
  if (gc->gc_controller->getBarNum() != MIN_NUM_GRID_BAR) {
    *gc->gc_controller->getBarNumPtr() /= 2;
  }
}

inline void GridControl::doubleSplit(GridControl* gc)
{
  if (gc->split_num != MAX_NUM_GRID_SPLIT) {
    gc->split_num *= 2;
  }
}

inline void GridControl::halfSplit(GridControl* gc)
{
  if (gc->split_num != MIN_NUM_GRID_SPLIT) {
    gc->split_num /= 2;
  }
}

} // namespace INVOXRecords
#endif // IR_GRIDCONTROL_H
