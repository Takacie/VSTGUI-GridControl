# VSTGUI GridControl

VST開発におけるVSTGUIを使ったGUI作成で使える画像のような感じのコントロールを追加できるヘッダファイルです。
VSTSDKバージョン3.7、VSTGUIバージョン4.10で作成しました。

![Screenshot](https://github.com/Takacie/VSTGUI-GridControl/blob/main/images/screenshot.png "screenshot")

# 使い方

以下の使い方は、VST3ProjectGeneratorで作成したプロジェクトであること、VSTGUIを表示させるためのクラス、つまり、Steinberg::Vst::VSTGUIEditorクラス、VSTGUI::IControlListenerクラスを継承したGUIEditorクラスが準備されていることを前提としています。（コード内の...は省略）

## Processorクラスでの準備

* 当ファイルをインクルードする
* ProcessorクラスにSteinberg::ITimerCallbackクラスを継承させ、純粋仮想関数のonTimer()をオーバーライドする

```c++
class XXXProcessor : public Steinberg::Vst::AudioEffect, public Steinberg::ITimerCallback
{
public:
...
  void onTimer(Steinberg::Timer* timer) override;
...
};
```

* メンバ変数として、DAWのプロジェクト時間を保持するための変数を宣言する

```c++
class XXXProcessor : public Steinberg::Vst::AudioEffect, public Steinberg::ITimerCallback
{
public:
...
private:
  Steinberg::Vst::TQuarterNotes project_time;
...
};
```

* process()内で現在のDAWのプロジェクト時間を取得する

```c++
...
if (data.processContext && data.processContext->state & data.processContext->kProjectTimeMusicValid) {
  project_time = data.processContext->projectTimeMusic;
}
...
```

* setupProcessing()でタイマーを動作させ、先程宣言したonTimer()内でcontrollerスレッドにプロジェクト時間の情報をメッセージで送る

```c++
tresult PLUGIN_API XXXProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
  if (!timer) timer = Steinberg::Timer::create(this, 5); // デストラクタでリリースするのを忘れずに
...
}
```

```c++
void XXXProcessor::onTimer(Steinberg::Timer* timer) {
  if (timer) GridControlController::sendHostInfoMessage(this, project_time);
  ...
}
```

## Controllerクラスでの準備

* 当ファイルをインクルードする
* ControllerクラスにSteinberg::ITimerCallbackクラス、VSTGUI::IControlListenerクラスを継承させ、純粋仮想関数のonTimer()、valueChanged()、ついでにprocessorスレッドから送られてきたメッセージを受け取る関数notify()をオーバーライドする

```c++
class XXXController : public Steinberg::Vst::EditControllerEx1, public Steinberg::ITimerCallback, public VSTGUI::IControlListener
{
public:
...
  tresult notify(Steinberg::Vst::IMessage* message) override;
  void onTimer(Steinberg::Timer* timer) override;
  void valueChanged(VSTGUI::CControl* pControl) override;
...
};
```

* メンバ変数として、GridControlControllerクラスを宣言し、GUIクラスで扱うためのゲッターを用意する

```c++
class XXXController : public Steinberg::Vst::EditControllerEx1, public Steinberg::ITimerCallback, public VSTGUI::IControlListener
{
public:
  GridControlController* getGridControllerPtr() { return gc_controller; }
...
private:
  GridControlController* gc_controller{ nullptr };
...
};
```

* initialize()内で、GridControlControllerクラスをインスタンス化し、タイマーを動作させる

```c++
tresult PLUGIN_API XXXController::initialize(FUnknown* context)
{
  gc_controller = new GridControlController(this, kParamID); // 第2引数はこのコントローラーで操作したいパラメーターのタグ
  timer = Steinberg::Timer::create(this, 5);
  // 双方ともデストラクタで解放するのを忘れずに
...
}
```

* notify()内でprocessorクラスから送られてきたメッセージを受け取り、プロジェクト時間を取得する

```c++
tresult XXXController::notify(Steinberg::Vst::IMessage* message)
{
  gc_controller->notifyHostInfoMessage(message);
...
}
```

* onTimer()内で、このコントロールのメインの処理を実行する(具体的には、GridControlControllerクラスで保持しているベクターとプロジェクト時間から、パラメーターの値を決めて変動させる処理)

```c++
void XXXController::onTimer(Timer* timer) {
  if (timer && gc_controller) {
    gc_controller->TimerProcess();
  }
...
}
```

* valueChanged()内で、前項のTimerProcess()で変動させた値を実際にパラメーターに反映させるようにする

```c++
void XXXController::valueChanged(CControl* pControl)
{
  Vst::ParamID index = pControl->getTag();
  Vst::ParamValue value = pControl->getValueNormalized();
  setParamNormalized(index, value);
  performEdit(index, value);
}
```

* (オプショナル)コントローラーの状態をプリセットに保存するため、setState()、getState()内で読み書きの処理を行う

```c++
tresult PLUGIN_API XXXController::setState(Steinberg::IBStream * state)
{
...
  if (gc_controller) {
    if (!gc_controller->setControlState(streamer)) return kResultFalse;
  }
...
}

```

```c++
tresult PLUGIN_API XXXController::getState(Steinberg::IBStream * state)
{
...
  if (gc_controller) {
    gc_controller->getControlState(streamer);
  }
...
}
```

## GUIクラスでの準備

* メンバ変数として、GridControlクラスを宣言する

```c++
class XXXGUIEditor : public Steinberg::Vst::VSTGUIEditor, public VSTGUI::IControlListener
{
public:
...
private:
  GridControl* grid_control = nullptr;
...
};
```

* open()内でGridControlクラスをインスタンス化し、フレームに追加する(コントロールが複数含まれているので、専用の関数を使うこと)

```c++
bool PLUGIN_API XXXGUIEditor::open(void* parent, const VSTGUI::PlatformType& platformType)
{
...
  // 第2引数はコントロールのサイズ、第3引数はcontrollerクラスの準備で作ったGridControlControllerクラスのゲッターで
  // GridControlControllerクラスのポインタを指定する
  grid_control = new GridControl(this, CRect(100, 100, 500, 400), dynamic_cast<XXXController*>(controller)->getGridControllerPtr());
  grid_control->addControlsToFrame(frame);
...
}
```

