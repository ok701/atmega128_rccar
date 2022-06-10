#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <math.h>

#include "LCD.h"

#define USControl_PORT PORTE		// 초음파센서 제어
#define USControl_DDR DDRE

#define DELAY_TIME 50 // 딜레이 타임
#define LENGTH 50		// 기준 거리 (cm)

volatile unsigned int pulse_count, pulse_count2 = 0;		// 58us 마다 증가되는 카운터변수
volatile unsigned char togle, togle2 = 0;


#define T 0.7 // 한바퀴 도는데 걸리는 시간
#define v 300 // 자동차 속도
#define pi 3.141592

volatile float L=0; // index L 초기화
volatile float R=0; // index R 초기화


//___이동 함수___
void Motor_enable(void);
void forward(void);
void backward(void);
void handlingRight(void);
void handlingLeft(void);
void stop(void);

void US1_pulse(void)		// 초음파 센서1 Trig핀에 10us input trigger pulse 출력
{
	USControl_PORT = 0x01;
	_delay_us(10);
	USControl_PORT = 0x00;
}

void US2_pulse(void)		// 초음파 센서2 Trig핀에 10us input trigger pulse 출력
{
	USControl_PORT = 0x02;
	_delay_us(10);
	USControl_PORT = 0x00;
}

//___타이머___
ISR(TIMER0_OVF_vect)		// Timer 0 Overflow Interrupt
{
	pulse_count++;		// 58us 마다 증가
	TCNT0 = 140;
}

ISR(TIMER2_OVF_vect)		// Timer 2 Overflow Interrupt
{
	pulse_count2++;		// 58us 마다 증가
	TCNT2 = 140;
}

//___에코핀 인터럽트___
ISR(INT4_vect)		// INT4핀에 Rising edge or Falling edge가 감지되면 인터럽트 호출
{
	if(togle == 0)		// Rising edge일때
	{
		pulse_count = 0;		// pulse_count 초기화

		TCCR0 = 0x02;		// Normal mode, Clock 8ck
		TCNT0 = 140;
		
		EICRB |= (1<<ISC41);		// Falling edge 감지
		EICRB &= 0xFE;
		
		TIMSK |= (1<<TOIE0);		
// Timer/Counter 0 Overflow Interrupt Enable

		togle = 1;
	}

	else		// Falling edge일때
	{
		EICRB |= (1<< ISC41)|(1<<ISC40);		// Rising edge 일 때 INT4_vect 인터럽트 호출
		
		TIMSK &= (1<< TOIE0);		// Timer/Counter 0 Overflow Interrupt Disable
		TCCR0 = 0x00;		// Timer/Counter 0 정지
		
		togle = 0;
	}
}

ISR(INT5_vect)		// INT5핀에 Rising edge or Falling edge가 감지되면 인터럽트 호출
{
	if(togle2 == 0)		// Rising edge일때
	{
		pulse_count2 = 0;		// pulse_count 초기화

		TCCR2 = 0x02;		// Normal mode, Clock 8ck
		TCNT2 = 140;
		
		EICRB |= (1<<ISC51);		// Falling edge 감지
		EICRB &= 0xFB;
		
		TIMSK |= (1<<TOIE2);		// Timer/Counter 2 이용
		
		togle2 = 1;
	}

	else		// Falling edge일때
	{
		EICRB |= (1<< ISC51)|(1<<ISC50);		// Rising edge 감지
		
		TIMSK &=0xBF;		// Timer/Counter 2 Overflow Interrupt Disable
		TCCR2 = 0x00;	   // Timer/Counter 2 정지
		
		togle2 = 0;
	}
}


//___main___
int main(void)
{
	//////속도 계산 인덱스들
	float theta = 0;
	float distanceX = 0; // x축으로 간 거리
	float distanceY = 0; // y축으로 간 거리
	
	char firstLine[] = "000000" ;
	char secondline[] = "000000";
	/////LCD끝////////
	LCDOUT();
	USControl_DDR = 0x0F;		// 초음파 센서 설정
	Motor_enable();		// 모터 레지스터 활성화
	DDRA = 0xFF;		// led 활성화
	//___Rising edge 일 때 INT4_vect, INT5_vect 인터럽트 호출___
	EIMSK |= (1<<INT4)|(1<<INT5);   // INT4,5 활성화
	EICRB |= (1<<ISC40)|(1<<ISC41)|(1<<ISC50)|(1<<ISC51);// INT4,5 Rising edge 활성화
	
	sei();		// 전체 인터럽트 Enable
	
	while(1)  //반복문 시작
	{
		US1_pulse();		// 초음파센서 1 동작
		_delay_ms(DELAY_TIME);
		
		US2_pulse();		// 초음파센서 2 동작
		_delay_ms(DELAY_TIME);
		
		
		Command(CLEAR);
		
		sprintf(firstLine, "x : %4.2f", distanceX);
		LCD_String(firstLine);
		sprintf(secondline, "y : %4.2f", distanceY);
		Command(LINE2);
		LCD_String(secondline);
		_delay_ms(100);
		
		
		if((pulse_count>LENGTH)&&(pulse_count2>LENGTH))
// 초음파 두개 모두 LENGTH 이상
		{
			theta = 90. + ((2.*180.) / T *0.05* (L+R)); 
//T는 차체가 한바퀴 도는데 걸리는 시간
			PORTA = 0x03;
			forward();
			_delay_ms(50);
			
			distanceX += 0.05 * v * cos(pi/180*theta);
			distanceY += 0.05 * v * sin(pi/180*theta);
		}
		else if((pulse_count>LENGTH)&&(pulse_count2<LENGTH))		     // 오른쪽 감지 좌회전
		{
			PORTA = 0x05;
			handlingLeft();
		}
		else if((pulse_count<LENGTH)&&(pulse_count2>LENGTH))		    // 왼쪽 감지 우회전
		{
			PORTA = 0x0A;
			handlingRight();
		}
		
		else		// 양쪽 감지 멈춤
		{
			PORTA = 0x0F;
			stop();
		}
	}
}

void Motor_enable(void) // 모터 레지스터 설정
{
	DDRF = 0x33;
	DDRB = 0x33;
}

void forward(void)// 전진
{
	PORTF=0x22;
	PORTB=0x21;
}

void backward(void)// 후진
{
	PORTF=0x21;
	PORTB=0x22;
}

void handlingRight(void)// 우회전
{
	PORTF=0x20;
	PORTB=0x01;
	_delay_ms(50);
	R--; //우회전 시 index R 감소
}

void handlingLeft(void)// 좌회전
{
	PORTF=0x02;
	PORTB=0x20;
	_delay_ms(50);
	L++; //좌회전 시 index L 증가
}

void stop(void)// 정지
{
	PORTF=0x00;// 신호선 2개 비트 같으면 0x00 또는 0x11
	PORTB=0x00;
}
