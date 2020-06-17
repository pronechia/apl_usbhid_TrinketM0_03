※2020/06/17 音のON/OFF判定強化

Arduinoを USB/HIDデバイスとして動作させるアプリ。
ボードは、Arduino互換の Adafruit Trinket M0　を使用する。
マイコンとブラウザーの間の双方向通信処理を実現する。
マイコンから定期的に、BME280の測定値のうち、ブラウザーからの指示に基づいた項目を表示するリアルタイムモニター。
ブラウザーからの指示は、Tone(音）を使って４bit/8bitの値をマイコンに送信する。

詳細はQiita参照 https://qiita.com/pronechia/items/d44b47b4c9667cea24ac


注意）

・Windows用とMac用で、ソースを分離しました。環境に合わせて利用してください。

  各自の環境にあわせて確認、修正する箇所があります。ソースコードの上部のコメントを参考に、対応してください。

・実行時の注意

  BME280のI2Cアドレスが間違っている場合は、値ゼロとなります。

  WindowsPC、Macとも、入力モードは、英数字である必要があります。日本語モードになっていないことを確認してください。

 This is an Arduino USB/HID with Tone sample program .
