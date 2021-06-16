# VSTGUI GridControl
VST開発におけるVSTGUIを使ったGUI作成で使える画像のような感じのコントロールを追加できるヘッダファイルです。
VSTSDKバージョン3.7、VSTGUIバージョン4.10で作成しました。

![Screenshot](https://github.com/Takacie/VSTGUI-GridControl/blob/main/images/screenshot.png "screenshot")

# 使い方
以下の使い方は、VST3ProjectGeneratorで作成したプロジェクトであること、VSTGUIを表示させるためのクラス、つまり、Steinberg::Vst::VSTGUIEditorクラス、VSTGUI::IControlListenerクラスを継承したクラスが準備されていることを前提としています。（コード内の...は省略）

## Processorクラスでの準備
* 当ファイルをインクルードする。
* ProcessorクラスにSteinberg::ITimerCallbackクラスを継承させ、純粋仮想関数のonTimer()をオーバーライドする。
```
class XXXProcessor : public Steinberg::Vst::AudioEffect, public Steinberg::ITimerCallback
{
public:
...
  void onTimer(Timer* timer) override;
...
```
* DAWのプロジェクト時間を保持するための変数を宣言する
```
class XXXProcessor : public Steinberg::Vst::AudioEffect, public Steinberg::ITimerCallback
{
public:
...
private:
  Steinberg::Vst::TQuarterNotes project_time;
...
```
* process()内で現在のDAWのプロジェクト時間を取得する
```
if (data.processContext && data.processContext->state & data.processContext->kProjectTimeMusicValid) {
  project_time = data.processContext->projectTimeMusic;
}
```
* setupProcessing()でタイマーを動作させ、先程宣言したonTimer()内でcontrollerスレッドにプロジェクト時間の情報をメッセージで送る
```
tresult PLUGIN_API XXXProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
  if (!timer) timer = Timer::create(this, 5); // デストラクタでリリースするのを忘れずに
  return AudioEffect::setupProcessing(newSetup);
}
```
```
void XXXProcessor::onTimer(Timer* timer) {
  if (timer) {
    GridControlController::sendHostInfoMessage(this, project_time);
  }
}
```

## Controllerクラスでの準備

## GUIクラスでの準備 

