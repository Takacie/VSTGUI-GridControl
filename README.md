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
class XXXProcessor : public Vst::AudioEffect, public ITimerCallback
{
public:
...
  void onTimer(Timer* timer) override;
...
```
* DAWのプロジェクトの時間を保持するための変数を宣言する
```
class XXXProcessor : public Vst::AudioEffect, public ITimerCallback
{
public:
...
private:
  void onTimer(Timer* timer) override;  
...
```

## Controllerクラスでの準備

## GUIクラスでの準備 

