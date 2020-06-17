/*
 *公開第三回）双方向リアルタイムモニター 2020/05/08
 * 下りデータは最大８bitだが、今回は4bitとした。
 * 2020/05/22 フォーカス異常改善
 * 2020/06/16 Tone ON/OFF判定強化
 */
// WindowsPC用
#define USBHOST_WINPC
//環境に合わせて内容を確認または修正すること　ーーここから -----------------------------
//①BME280 I2Cアドレスの設定　（0x76 or 0x77 のどちらか）
#define BME280DEVADDR 0x76
//#define BME280DEVADDR 0x77
//②Webserverに合わせて、ＵＲＬを記述すること
//String g_url_string = "http'//192.168.xxx.xxx/xxxxxxxx.htm"; //内部WebServer用
String g_url_string = "http'//localhost/example0301.htm"; //localhost用
//③ハード設定
#define LED_PIN 13
#define SW_PIN  1
#define TONE_PIN  4
//環境に合わせて内容を確認または修正すること　ーーここまで -----------------------------

#define WCS_DELAY_T1 150 //T1 ブラウザー応答時間150
#define WCS_DELAY_GAMEN 3000  //URLたたいてから画面が表示されるまで待つ
#define WCS_BITREAD_MAXCOUNT 5  //bit read のリトライ回数上限
#define WCS_SWWAIT_MAXCOUNT 30  //sw押下待ちのリトライ回数上限 30秒
#include <Wire.h>
#include <ZeroTimer.h>
#include <SparkFunBME280.h>
#include "lib_keyboard.h"
#include "ToneManager.h"
#include "VolumeControl.h"

BME280 mySensor;
ToneManager myToneM;
VolumeControl myVolumeC;

int16_t g_pass;  //HID出力したら後の待ち時間を制御する
boolean g_I2CNormal;
volatile int g_i; //timerカウント
volatile boolean g_high_spead;
int g_dataSyubetu = 1; //1-4。初期値は温度
int g_retry_count;
//
//
//
void setup(){  
  g_pass = 1;
  g_i = 0;
  g_high_spead = 0;
  g_retry_count = 0;
  pinMode(LED_PIN,OUTPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  myToneM.begin(TONE_PIN, 8);  //8bit

  Wire.begin();
  delay(100); //I2Cデバイスの初期化待ち
  mySensor.setI2CAddress(BME280DEVADDR);
  //BME280の初期化ができない場合、値ゼロで動かす
  if (mySensor.beginI2C() == false) g_I2CNormal = false;
  else                              g_I2CNormal = true;
  //
  sub_fw_Blink(LED_PIN, 3, 50); //動き始めたことを知らせる
  //
  //SWが押されるまで待つ 10msec x 100 = 1sec でエラーとする
  boolean sw_pushed = false;
  for (int j = 0; j< (WCS_SWWAIT_MAXCOUNT * 100); j++) {  
    if (sub_fw_SWcheck(SW_PIN) == 1) {
      sw_pushed = true;
      break;
    }
    delay(10);
  }
  if (sw_pushed == false )
    while(1) sub_fw_Blink(LED_PIN, 10, 50);  //LED点滅し続ける
  
  // 音量UPのため、HIDデバイスを定義する
  myVolumeC.begin();
#if defined USBHOST_WINPC
  myVolumeC.volumeUP(1);  //windows
#elif defined USBHOST_MAC
  myVolumeC.volumeUP(2);  //MAC
#else
  // 何もしない。後で音が拾えない可能性がある
#endif

  //
  delay(1000);  //USB/HID再定義のための待ち時間。
#if defined USBHOST_WINPC
  sub_kbd_begin(1);  //Windows用に初期化
#elif defined USBHOST_MAC
  sub_kbd_begin(2);  //Mac用に初期化
#else
  // 何もしない。後でKEYBOARDが動作しない可能性大
#endif
  delay(100); //HIDデバイスの初期化待ち
  //
  sub_fw_timerset();  //タイマー起動
}
//
//main loop
//
void loop(){
  // 40msec毎に処理を行う
  if (sub_fw_event(2)) sub_proc();
}
//
//USB/HID処理
//
void sub_proc() {
  static byte s_command = 0; //処理振り分け
  static byte s_first = 0;
  int j;
  
  //待ち時間がゼロになるまで何もしない。
  if (g_pass-- >= 0 ) return;
  //処理振り分け
  if (s_command == 0) { 
    s_command = 1;
    s_first = 1;
    sub_initurl(); //一回だけ実施
  } else if (s_command == 1) { 
    sub_out_kbd(1);  //start
    if (sub_check_tone()) s_command = 20;
    else s_command = 90;
  } else if (s_command == 10) {
    sub_out_kbd(10); //Data
    if (sub_check_tone()) s_command = 11;
    else s_command = 90;
  } else if (s_command == 11) {
    sub_out_kbd(11); //センサー値UP
    if (sub_check_tone()) s_command = 20;
    else s_command = 90;
  } else if (s_command == 20) {
    sub_out_kbd(20); //Inqu
    if (sub_check_tone()) s_command = 21;
    else s_command = 90;
    g_retry_count = 0; //次回のために初期化  
    myToneM.clear();
  } else if ((s_command >= 21) && (s_command <= 28)) {
    //制御情報を1ビットつづ受けとる。８ビット
    sub_out_kbd(s_command); //Inq no1 to no8
    g_high_spead = 1; //問い合わせは待ち時間なしで動かす。本当はS_command==21のみでいい
    if (myToneM.readBit(s_command - 20)) {
      //正常
      s_command = s_command+1;
      g_retry_count = 0; //次回のために初期化  
      //8bitは無駄なので、4bitで次へ
      if (s_command == 25 ) s_command = 29; 
    } else {
      //異常時は同じビットを再送要求する
      if (g_retry_count++ >= WCS_BITREAD_MAXCOUNT) s_command = 90; //リトライオーバーで異常へ
    }
  } else if (s_command == 29) { 
    g_high_spead = 0; //問い合わせは待ち時間なしで動かすモードを終了する
    //制御情報を1ビットつづ受けとる。８ビット
    sub_out_kbd(29); //Inqe end
    g_dataSyubetu = myToneM.getToneVal(); //受信した制御値を取り出す
    s_command = 10; //次のセンサーデータUPへ、
  } else if (s_command == 90) {
    //フォーカスがおかしくて音が拾えない場合にここにくる
    sub_out_kbd(90); //フォーカスをコマンドフィールドへ
    s_first = 1;
    s_command = 1;   //startコマンドから再開する     
  } else {
    //その他はエラー
    sub_fw_Blink(LED_PIN, 10, 40);
  }
  
  if (s_command == 1) {
    g_pass = 50; //40msec*50= 2sec
  } else if (s_command == 20) {
    if (s_first == 1) {
      g_pass = 50; //40msec*50= 2sec
      s_first = 0;
    } else {
      g_pass = 200; //40msec*200= 8sec
    }
  } else if ((s_command > 9) && (s_command < 12)) {
    g_pass = 25; //40msec*25= 1sec
  } else if ((s_command > 20) && (s_command < 30)) {
    g_pass = 1; //high speed
  } else if (s_command == 90) {
    g_pass = 25; //40msec*25= 1sec
  } else {
    //フォーカスおかしいときも、一旦ここにきて待つ
    g_pass = 200; //40msec*200= 8sec
  }
}
//
//周期によって、決まった文字列処理（主にHTML画面への文字列入力）をおこなう
//
void sub_out_kbd(int8_t p_ctl) {
  int32_t w_temp;
  int32_t w_temp2;
  char w_buf[16];
  char w_buf2[16];
  char w_buf3[16];

  if (p_ctl == 0) {
    delay(WCS_DELAY_T1 );
  } else if (p_ctl == 1) {
    sub_moji_tab("Start");
    sub_kbd_strok(KEY_RETURN);
  } else if (p_ctl == 10) {
    sub_moji_tab("Data");
    sub_kbd_strok(KEY_RETURN);
  } else if (p_ctl == 11) {
    //ラベルと値を出力する
    sub_out_val(g_dataSyubetu, w_buf ,w_buf2, w_buf3);
    sub_moji_tab(w_buf);  //ラベル
    sub_moji_tab(w_buf2);  //値
    sub_moji_tab(w_buf3);  //単位
    sub_kbd_strok(KEY_RETURN);
  } else if (p_ctl == 20) {
    //制御情報を問い合わせる
    sub_moji_tab("Inqu");
    sub_kbd_strok(KEY_RETURN);
  } else if ((p_ctl > 20) && (p_ctl < 29)) {
    w_temp = p_ctl - 20;
    sprintf(w_buf, "%d", w_temp);
    sub_moji_tab(w_buf);
    sub_kbd_strok(KEY_RETURN);
  } else if (p_ctl == 29) {
    sub_moji_tab("e");
    sub_kbd_strok(KEY_RETURN);
  } else if (p_ctl == 90) {
    //OSに依存せず、initurl()に統一する
    //URLを再入力する
    sub_initurl();
    //URLフィールドへ移動+Tabでフォーカスをコマンドフィールドへ移す
    //sub_kbd_toCommandF();
  } else {
    sub_moji_tab("error");
    sub_kbd_strok(KEY_RETURN);
  }
}
//
//50msec*5回繰り返す。音が拾えたらrc=trueとなる
boolean sub_check_tone(void) {
  int j;

  //はじめは音が鳴っていないことを確認する
  if (myToneM.judgeTone()) {
    // (50msec x 2(on/OFF) x 50回)= 5sec 
    sub_fw_Blink(LED_PIN, 50, 50);
    return false;
  }
  //
  for (j=0; j<5; j++) {
    delay(50);   
    if (myToneM.judgeTone()) return true;
  }
  //エラー検知
  // (50msec x 2(on/OFF) x 10回)= 1sec 
  sub_fw_Blink(LED_PIN, 10, 50);
  return false; //50msecX5=250msecでも音が鳴らないため、3秒点滅後、エラーを返す    
}
//
//アプリ画面を呼び出すURLを出力する
void sub_initurl() {

  //URL入力前に入力エリアにフォーカスを当てる
  sub_kbd_preURL();
  delay(WCS_DELAY_T1);
  //URL文字出力
  sub_kbd_print((char *)g_url_string.c_str());
  sub_kbd_strok(KEY_RETURN);
  //
  delay(WCS_DELAY_GAMEN); //画面が表示されるまで、十分の時間待つ
  sub_kbd_strok(KEY_TAB);
}
//タイマー割込み関数
void sub_timer_callback() {
  // 指定した間隔(default 20msec)で割込みが入る
  g_i++;
}
//
//指示されたデータ種別の文字列を出力する
//
void sub_out_val(int p_datasyubetu, char* p_1 ,char* p_2, char* p_3) {
  char w_buf[16];
  int32_t w_temp;
  int32_t w_temp2;

  if (p_datasyubetu == 1) {
    strcpy(p_1, "Temprature");
    //センサー値を得る
    w_temp = (int32_t)(mySensor.readTempC() * 100); //xx.xx度をxxxxに変換
    if (w_temp > 9999) strcpy(w_buf, "100.00");
    else {
      w_temp2 = w_temp - (w_temp / 100 * 100);
      sprintf(w_buf, "%d.%02d", w_temp / 100, w_temp2);
    }
    strcpy(p_2, w_buf);
    strcpy(p_3, "C");
  } else if (p_datasyubetu == 2) {
    sprintf(p_1, "Humidity");
    //センサー値を得る
    w_temp = (int32_t)(mySensor.readFloatHumidity() * 100); //xx.xx%をxxxxに変換
    if (w_temp > 9999) strcpy(w_buf, "100.00");
    else {
      w_temp2 = w_temp - (w_temp / 100 * 100);
      sprintf(w_buf, "%d.%02d", w_temp / 100, w_temp2);
    }
    sprintf(p_2, "%s", w_buf);
    //NG sprintf(p_3, "\%");  //特殊文字
    strcpy(p_3, "%");
  } else if (p_datasyubetu == 3) {
    strcpy(p_1, "pressure");
    //センサー値を得る
    w_temp = (int32_t)((mySensor.readFloatPressure() + 50.0) / 100.0); //1013hPa
    if (w_temp > 1999) strcpy(w_buf, "2000");
    else sprintf(w_buf, "%4d", w_temp);
    strcpy(p_2, w_buf);
    strcpy(p_3, "hPa");

  } else if (p_datasyubetu == 4) {
    strcpy(p_1, "DI");
    //温度、湿度から不快指数を計算する
    float w_t;
    float w_h;
    float w_w;
    int w_out;
    //0.81T + 0.01H(0.99T - 14.3 ) + 46.3
    w_t = mySensor.readTempC();
    w_h = mySensor.readFloatHumidity();
    w_w = (float)0.99 * w_t - (float)14.3;
    w_w = (float)0.81 * w_t + (float)0.01 * w_h * w_w + (float)46.3;
    w_out = w_w + (float)0.5; //不快指数を整数で丸める      
    if (w_out > 100) strcpy(w_buf, "100");
    else sprintf(w_buf, "%d", w_out);
    strcpy(p_2, w_buf);
    strcpy(p_3, "");  //不快指数の単位はない
  } else {
    //その他は空白
    strcpy(p_1, "syubetu");
    w_temp = g_dataSyubetu;
    sprintf(w_buf, "%d", w_temp);
    strcpy(p_2, w_buf);
    strcpy(p_3, "ERROR");  //単位欄
  }
}
//
void sub_fw_timerset() {
  //20msec毎
  TC.startTimer(20000, sub_timer_callback);
}
//2カウント=約40msecでイベントを上げる
// rc true =event on
boolean sub_fw_event(uint8_t p_count) {
  boolean w_trriger;

  if (p_count > 25) return 0; //ERROR check  
  w_trriger = false;
  // g_i>50で約一秒。g_i>0で約20msec
  if (g_i >= p_count) {
    g_i = 0;
    w_trriger = true;
  }
  return w_trriger;
}
//
//SWが押されたかのチェック。二度呼ばれて、連続して押されていたらONを返す。
// rc 0:OFF 1:立ち上がり 2:立下り
uint8_t sub_fw_SWcheck(uint8_t p_swpin) {
  static boolean s_pre_sw = 1; // =1はOFFを意味する
  static uint8_t s_first_sw = 0;
  boolean w_sw;

  w_sw = digitalRead(p_swpin); // =0がSWを押したとき
  //前回がOFF->ONの時、連続か？
  if (s_first_sw == 1) {
    s_first_sw = 0;
    s_pre_sw = w_sw;  //次回のために
    if (w_sw == 0) return 1; //ON->ON つまり連続して押された
    else return 0;           // ON->OFFだった。
  //前回がON->OFFの時、連続か？
  } else if (s_first_sw == 2) {
    s_first_sw = 0;
    s_pre_sw = w_sw;  //次回のために
    if (w_sw == 1) return 2; //OFF->OFF つまり連続して離れた
    else return 0;           // OFF->ONだった。
  }
  //OFFからONを検知する。
  if ((s_pre_sw == 1) && (w_sw == 0)) {
    s_first_sw = 1;
  }
  //ONからOFFを検知する。
  if ((s_pre_sw == 0) && (w_sw == 1)) {
    s_first_sw = 3;
  }
  s_pre_sw = w_sw;  //次回のために
  return 0;
}
//p_timesは最大255とする
void sub_fw_Blink(uint8_t p_ledpin, byte p_times , int p_wait){
  for (byte i=0; i< p_times; i++){
    digitalWrite(p_ledpin, LOW);
    delay (p_wait);
    digitalWrite(p_ledpin, HIGH); //点灯で終わる
    delay (p_wait);
  }
}
