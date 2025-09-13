//https://www.youtube.com/watch?v=ZDfZmj8liBU 
//Sensor
#define CLK PA0
#define SI PA1
volatile unsigned int camera_data[140] = {0};
int s_cnt = 0; // send counter

// Step moter
#define EN_PIN 17   // /EN (Enable)
#define STEP_PIN_1 21  // Step1 PD0
#define DIR_PIN_1 20   // /Dir1 PD1
#define STEP_PIN_2 19  // Step2 PD2
#define DIR_PIN_2 18 // /Dir2 PD3

int cnt1 =0;
int cnt2 =0;
int cnt3 =0;
int cnt4 =0;
volatile int a = 12800;
volatile int b1 = 6400;  volatile int b2 = 6400;
volatile int M1 = ((a/b1) >> 1) ; volatile int M2 = ((a/b2) >> 1);

// Recognize black line
//  ref : (falling edge)흰색과 검은색을 구분하기 위한 상수
// k1 : 왼쪽 검은선의 위치를 저장하는 변수
// k2 : 오른쪽 검은선의 위치를 저장하는 변수
int ref = 200;  
int k1 = 0;
int k2 = 0;

// way: 방향설정 변수
int way = 0;
// 비례 제어 상수
int kp = 100;


void timer_init() 
{
  TCCR1A &= ~_BV(WGM10);  //  CTC Mode
  TCCR1A &= ~_BV(WGM11);  
  TCCR1B |= _BV(WGM12);
  TCCR1B &= ~_BV(WGM13);
  TCCR1B |= _BV(CS10);
  TCCR1B &= ~_BV(CS11);  // prescalar = 1
  TCCR1B &= ~_BV(CS12);
  OCR1A = 1650;  // 타이머 인터럽트의 발생 주기로 이 값을 통해 속도를 조절할 수 있다. 
  TCNT1 = 0x0000;
  TIMSK1 |= _BV(OCIE1A);
  TIFR1 |= _BV(OCF1A);
}

void ADCInit()
{
    // ADMUX - ADC Multiplexer Selection Register
    // Bit 7:6 - REFS1:0 : Reference Volatage Selection Bits
    // Bit 5 - ADLAR : ADC LEFT Adjust Result
    // Bit 4:0 - MUX4:0 : Analog Channel and Gain Selection Bits
    ADMUX = 0;
    ADMUX &= ~_BV(REFS1);
    ADMUX |= _BV(REFS0);
    ADMUX &= ~_BV(ADLAR);  // 오른쪽 정렬

    // ADCSRA - ADC Control and Status Register A
    // Bit 7 - ADEN : ADC Enable
    // Bit 6 - ADSC : ADC Start Conversion
    // Bit 5 - ADATE : ADC Auto Trigger Enable
    // Bit 4 - ADIF : ADC Interrupt Flag
    // Bit 3 - ADIE : ADC Interrupt Enable
    // Bit 2:0 - ADPS2:0 : ADC Prescaler Select Bits
    ADCSRA |= _BV(ADEN);
    ADCSRA &= ~_BV(ADSC);
    ADCSRA &= ~_BV(ADATE);
    ADCSRA |= _BV(ADIF);
    ADCSRA &= ~_BV(ADIE);
    ADCSRA |= _BV(ADPS2);
    ADCSRA &= ~_BV(ADPS1);
    ADCSRA |= _BV(ADPS0);
}

// ADC_Get : 밝기 정보를 camera_data[num]에 저장하는 함수
void ADC_Get(unsigned char num)
{
  ADCSRA |= (1<<ADSC); // Start conversion
  while(ADCSRA & (1<<ADSC));
  camera_data[num] = ADCW;
}

// ISR : DDR알고리즘으로 Step Motor를 구동
ISR(TIMER1_COMPA_vect){ 
cnt1++;  // For step 1
cnt3++;  // For step 2

// Step Motor 1(왼쪽 바퀴) DDA
if( cnt1 > M1 ) PORTD &= 0b11111011;
cnt2 = cnt2 + b1;

if(cnt2>=a){
PORTD |= 0b00000001;
cnt2 = cnt2-a;
cnt1 = 0;
}

// Step Motor 2(오른쪽 바퀴)DDA
if(cnt3 > M2) PORTD &= 0b11111110;
cnt4 = cnt4 + b2;
  if(cnt4>=a){
  PORTD |= 0b00000100;
  cnt4 = cnt4-a;
  cnt3 = 0;
  }
}

void setup() {
  // Setting I/O
  PORTD &= 0b11110101; 
  PORTD |= 0b00001101;
  DDRD |= 0b00001111;
  DDRH |= 0b00000001;
  Serial.begin(115200);
  ADCInit();
  DDRA = (1<<CLK) | (1<<SI);
  timer_init();
  sei();
}

void loop() {
  int i = 0;

  // Line scan camera 읽어오기
  PORTA |= (1<<SI);
  PORTA |= (1<<CLK);
  PORTA &= ~(1<<SI);
  delay(1);

  for(i = 0; i < 128 ; i++)
  {
    ADC_Get(i);
    PORTA &= ~(1<<CLK);
    PORTA |= (1<<CLK);
  }
  PORTA &= ~(1<<CLK);
  s_cnt++;
  if(s_cnt >= 7) s_cnt=0;
// -----------------------------------
// 64(렌즈 중앙)을 기준으로 왼쪽으로 이동하며 가장 먼저
// falling edge(검은색)이 관측되는 위치를 k1에 저장
  for(int j = 64 ; j>=0 ; j--) {
    if(camera_data[j] <= ref) {
      k1 = j;
      break;
    }
  }

// 64(렌즈 중앙)을 기준으로 오른쪽으로 이동하며 가장 먼저
// falling edge(검은색)이 관측되는 위치를 k2에 저장
  for(int j = 64 ; j<=128 ; j++) {
    if(camera_data[j] <= ref) {
      k2 = j;
      break;
    }
  }

//  ------------------------------------------------------
// 모터의 속도 회전을 결정하는 부분
// 기본 속도 : 6400
// b1 : 왼쪽 바퀴 속도, b2 : 오른쪽 바퀴 속도
// M1, M2 : 변한 b1, b2에 맞추어 새로운 M값 설정
// way : 회전 방향을 결정하는 flag
// kp : 비례 제어 상수
// k1 : 왼쪽 검은선, k2 : 오른쪽 검은선

// 렌즈에서 검은선이 하나만 검출되기 이전에 직진 상태에서
// 중앙을 기준으로 일정 거리 이상되는 지점에서 검은선이 검출되기 시작하면
// 회전방향을 way변수를 통해 지정해준다.
  // Turn LEFT
  if(way == -1) {
    // k2가 작아질수록(오른쪽 검은선이 중앙으로 다가올수록)
    // Left Motor 속도는 빨라지고 Right Motor 속도는 느려진다. 
    b1 = 6400+kp*(128-k2);
    b2 = 6400-kp*(128-k2);
    M1 = (a/b1)>>1;
    M2 = (a/b2)>>1;
    // k2가 일정거리 이상 멀어지면 다시 직진
    if (k2>=108) way = 0;
  }
  // Turn Right
  if(way == 1) {
    // k1이 커질수록(왼쪽 검은선이 중앙으로 다가올수록)
    // Left Motor 속도는 느려지고, Right Motor 속도는 빨라진다.
    b1 = 6400-kp *k1;
    b2 = 6400+kp*k1;
    M1 = (a/b1)>>1;
    M2 = (a/b2)>>1;
    // k1이 일정거리 이상 멀어지면 다시 직진
    if(k1 <= 18) way = 0;
  }
  // Go Straight
  else if(way == 0) {
    // 양쪽 모터 속도를 기본속도로 초기화
    b1=6400;
    b2=6400;
    M1=(a/b1)>>1;
    M2 =(a/b2)>>1;
    // k2가 118이하의 값에서 처음 검출되기 시작하면
    // 오른쪽 검은선이 점점 중앙쪽으로 다가오고 있다는 뜻으로
    // 좌회전을 의미하는 -1을 way변수에 저장
    if(k2 <= 118) way = -1;  //  Turn LEFT
    // k1이 10이상의 값에서 처음 검출되기 시작하면
    // 왼쪽 검은선이 점점 중앙쪽으로 다가오고 있다는 뜻으로
    // 우회전을 의미하는 1을 way변수에 저장
    if(k1 >= 10) way = 1;  //  Turn Right
  }
}
