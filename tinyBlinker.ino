  //#include <TinyDebugSerial.h>
//TinyDebugSerial mySerial = Serial();

#include <SoftwareSerial.h>
#include <EEPROM.h>

#define led 1
#define rx 2
#define tx 0

#define ver 204
/*
  #define tl 900
  #define ts 300
  #define tw 100
*/

struct Params {
  uint8_t  cal;
  uint16_t tl;
  uint16_t ts;
  uint16_t tw;
  uint16_t iu;
  uint16_t il;
  uint16_t imin;
  bool     dirty;
};

char buf[5];
uint8_t buf_p;

#define eebase 0

SoftwareSerial mySerial(rx,tx);

Params myParams;
uint16_t eeaddr=eebase;
void setup() {

  EEPROM.get(eeaddr,myParams);

  
  if ((myParams.tl == 0xffff && myParams.ts == 0xffff  && myParams.tw == 0xffff)||(myParams.tl==0x0000 && myParams.ts==0x000)){
    myParams.tl = 900;
    myParams.ts = 300;
    myParams.tw = 100;
    myParams.imin=40;
    myParams.iu=10; // default starting values for the U/I
    myParams.il=23; // curve to detect single missing lamps
    if(myParams.cal==0xff) myParams.cal=0x85;
    myParams.dirty=true; 
  }

  
  OSCCAL=myParams.cal;
  delay(20);
  mySerial.begin(9600);
  
  if (myParams.il == 0 or myParams.il == 0xffff){
    myParams.iu == 8;
    myParams.il == 10;
    myParams.dirty = true; 
  }
  
  if(myParams.dirty){
    myParams.dirty = false;
    EEPROM.put(eeaddr,myParams);
    Serial.println("saved Params");
  }

  pinMode(led,OUTPUT);
  //pinMode(2,INPUT);
  pinMode(3,INPUT);
  pinMode(4,INPUT);
  

  mySerial.println(F("starting up"));

  mySerial.println(F("read timing from EEPROM"));
  mySerial.print(F("tl: "));mySerial.println(myParams.tl);
  mySerial.print(F("ts: "));mySerial.println(myParams.ts);
  mySerial.print(F("tw: "));mySerial.println(myParams.tw);
  mySerial.println();
  
  ADMUX = 0b10010111; //set ADC
  digitalWrite(led,HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  int i,v;
  uint8_t low,high;

  ADMUX = 0b10010111; //set ADC to differential mode adc2 adc3 with 20x gain and 2.56V VREF
  ADCSRB &= ~(1<<7); // set input to unipolar

  delay(10);
  ADCSRA|=(1<<ADSC);
  while(ADCSRA&(1<<ADSC));
  low=ADCL;
  high=ADCH;
  i=(high<<8)|low;

  ADMUX = 0b00010010; //set ADC to single ended mode adc2 and VCC VREF
  delay(10);
  ADCSRA|=(1<<ADSC);
  while(ADCSRA&(1<<ADSC));
  low=ADCL;
  high=ADCH;
  v=(high<<8)|low;
  
  if(i<myParams.imin){
    delay(myParams.tw);
  }else if(i>v*myParams.iu/myParams.il){
    delay(myParams.tl/2);
    digitalWrite(led,LOW);
    delay(myParams.tl/2);
    digitalWrite(led,HIGH);
  }else{
    delay(myParams.ts/2);
    digitalWrite(led,LOW);
    delay(myParams.ts/2);
    digitalWrite(led,HIGH);
  }

  if(mySerial.available()){
    char c=mySerial.read();
    mySerial.println(c);
    switch(c){
      case 'l':
        mySerial.print(F("read V: "));
        mySerial.print(v);
        mySerial.print(F(" I: "));
        mySerial.println(i);
        break;
      case 's':
        mySerial.print(F("enter new long interval value [ms]: ("));
        mySerial.print(myParams.tl);
        mySerial.print(F(") "));
        myParams.tl=getVal(myParams.tl);
        mySerial.print(F("enter new short interval value [ms]: ("));
        mySerial.print(myParams.ts);
        mySerial.print(F(") "));
        myParams.ts=getVal(myParams.ts);
        mySerial.print(F("enter new sleep value [ms]: ("));
        mySerial.print(myParams.tw);
        mySerial.print(F(") "));
        myParams.tw=getVal(myParams.tw);
        mySerial.print(F("enter new multiplicant value: ("));
        mySerial.print(myParams.iu);
        mySerial.print(F(") "));
        myParams.iu=getVal(myParams.iu);
        mySerial.print(F("enter new divisor value: ("));
        mySerial.print(myParams.il);
        mySerial.print(F(") "));
        myParams.il=getVal(myParams.il);
        mySerial.print(F("enter new min on value: ("));
        mySerial.print(myParams.imin);
        mySerial.print(F(") "));
        myParams.imin=getVal(myParams.imin);
        EEPROM.put(eeaddr,myParams);
        break;
      case 'v':
        mySerial.print(F("blinker version "));
        mySerial.println(ver);
        break;
      case 'r':
        mySerial.print(F("osccal:"));
        mySerial.println(myParams.cal);
        mySerial.print(F("long int:"));
        mySerial.println(myParams.tl);
        mySerial.print(F("short int:"));
        mySerial.println(myParams.ts);
        mySerial.print(F("sleep int:"));
        mySerial.println(myParams.tw);
        mySerial.print(F("multiplicant:"));
        mySerial.println(myParams.iu);
        mySerial.print(F("divisor:"));
        mySerial.println(myParams.il);
        mySerial.print(F("minimum on current:"));
        mySerial.println(myParams.imin);
        break;
      case '?':
      default:
        mySerial.println(F("l: show last voltage and current reading"));
        mySerial.println(F("r: recall calibration data"));
        mySerial.println(F("s: set calibration data"));
        mySerial.println(F("v: print firmware version"));
        break;
    }
  }
}

uint16_t getVal(uint16_t v_){
  buf_p=0;
  uint16_t v;
  uint8_t l;
  while(buf_p<5){
    while(!mySerial.available());
    char c=mySerial.read();
    if(c>=0x30 && c<=0x39) {
      buf[buf_p++]=c;
    }else if(c==0x0d){
      buf[buf_p]=0x00;
      v=atoi(buf);
      if(v==0) v=v_;
      mySerial.println(v);
      return v;
    }
  }
}


