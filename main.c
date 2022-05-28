/*
 * Digital Clock.c
 *
 * Created: 4/23/2022 10:55:51 AM
 * Author : Orhan Ozbasaran
 */ 

#include <avr/io.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <stdio.h>

#define XTAL_FRQ 8000000lu

#define SET_BIT(p,i) ((p) |=  (1 << (i)))
#define CLR_BIT(p,i) ((p) &= ~(1 << (i)))
#define GET_BIT(p,i) ((p) &   (1 << (i)))

#define WDR() asm volatile("wdr"::)
#define NOP() asm volatile("nop"::)
#define RST() for(;;);

#define DDR    DDRB
#define PORT   PORTB
#define RS_PIN 0
#define RW_PIN 1
#define EN_PIN 2

void avr_wait(unsigned short msec)
{
	TCCR0 = 3;
	while (msec--) {
		TCNT0 = (unsigned char)(256 - (XTAL_FRQ / 64) * 0.001);
		SET_BIT(TIFR, TOV0);
		WDR();
		while (!GET_BIT(TIFR, TOV0));
	}
	TCCR0 = 0;
}

static inline void
set_data(unsigned char x)
{
	PORTD = x;
	DDRD = 0xff;
}

static inline unsigned char
get_data(void)
{
	DDRD = 0x00;
	return PIND;
}

static inline void
sleep_700ns(void)
{
	NOP();
	NOP();
	NOP();
}

static unsigned char
input(unsigned char rs)
{
	unsigned char d;
	if (rs) SET_BIT(PORT, RS_PIN); else CLR_BIT(PORT, RS_PIN);
	SET_BIT(PORT, RW_PIN);
	get_data();
	SET_BIT(PORT, EN_PIN);
	sleep_700ns();
	d = get_data();
	CLR_BIT(PORT, EN_PIN);
	return d;
}

static void
output(unsigned char d, unsigned char rs)
{
	if (rs) SET_BIT(PORT, RS_PIN); else CLR_BIT(PORT, RS_PIN);
	CLR_BIT(PORT, RW_PIN);
	set_data(d);
	SET_BIT(PORT, EN_PIN);
	sleep_700ns();
	CLR_BIT(PORT, EN_PIN);
}

static void
write(unsigned char c, unsigned char rs)
{
	while (input(0) & 0x80);
	output(c, rs);
}

void
lcd_init(void)
{
	SET_BIT(DDR, RS_PIN);
	SET_BIT(DDR, RW_PIN);
	SET_BIT(DDR, EN_PIN);
	avr_wait(16);
	output(0x30, 0);
	avr_wait(5);
	output(0x30, 0);
	avr_wait(1);
	write(0x3c, 0);
	write(0x0c, 0);
	write(0x06, 0);
	write(0x01, 0);
}

void
lcd_clr(void)
{
	write(0x01, 0);
}

void
lcd_pos(unsigned char r, unsigned char c)
{
	unsigned char n = r * 40 + c;
	write(0x02, 0);
	while (n--) {
		write(0x14, 0);
	}
}

void
lcd_put(char c)
{
	write(c, 1);
}

void
lcd_puts1(const char *s)
{
	char c;
	while ((c = pgm_read_byte(s++)) != 0) {
		write(c, 1);
	}
}

void
lcd_puts2(const char *s)
{
	char c;
	while ((c = *(s++)) != 0) {
		write(c, 1);
	}
}

int is_pressed(int r, int c) {
	// Set all 8 GPIOs to N/C
	DDRA = 0;
	PORTA = 0;
	SET_BIT(DDRA, 4 + r);
	SET_BIT(PORTA, c);
	avr_wait(1);
	if (!GET_BIT(PINA, c)) {
		return 1;
	}
	return 0;
}

int get_key() {
	int i, j;
	for (i=0; i < 4; i++) {
		for (j=0; j < 4; j++) {
			if (is_pressed(i, j)) {
				return 4 * i + j + 1;
			}
		}
	}
	return 0;
}

typedef struct {
	int year;
	unsigned char month;
	unsigned char day;
	int hour;
	int minute;
	int second;
} DateTime;

void init_dt(DateTime *dt) {
	dt->year = 2024;
	dt->month = 02;
	dt->day = 29;
	dt->hour = 23;
	dt->minute = 59;
	dt->second = 50;
}

void check_dt(DateTime *dt) {
	if (dt->second >= 60) {
		dt->second = 0;
		++dt->minute;
	}
	if (dt->second <= -1) {
		dt->second = 59;
		--dt->minute;
	}
	if (dt->minute >= 60) {
		dt->minute = 0;
		++dt->hour;
	}
	if (dt->minute <= -1) {
		dt->minute = 59;
		--dt->hour;
	}
	if (dt->hour >= 24) {
		dt->hour = 0;
		++dt->day;
	}
	if (dt->hour < 0) {
		dt->hour = 23;
		--dt->day;
	}
	if((dt->day >= 32) && (dt->month==1 || dt->month==3 || dt->month==5 || dt->month==7 || dt->month==8 || dt->month==10 || dt->month==12)) {
		dt->day = 1;
		++dt->month;
	}
	if((dt->day <= 0) && (dt->month==1 || dt->month==3 || dt->month==5 || dt->month==7 || dt->month==8 || dt->month==10 || dt->month==12)) {
		if (dt->month==3) {
			if (dt->year %400==0 ||(dt->year%4==0 && dt->year%100!=0)) {
				dt->day = 29;
			}
			else {
				dt->day = 28;
			}
		}
		else if (dt->month==1) {
			dt->day = 31;
		}
		else {
			dt->day = 30;
		}
		--dt->month;
	}
	if((dt->day >= 31) && (dt->month==4 || dt->month==6 || dt->month==9 || dt->month==11)) {
		dt->day = 1;
		++dt->month;
	}
	if((dt->day <= 0) && (dt->month==4 || dt->month==6 || dt->month==9 || dt->month==11)) {
		dt->day = 31;
		--dt->month;
	}
	if((dt->day >= 30 && dt->month == 2) && (dt->year %400==0 ||(dt->year%4==0 && dt->year%100!=0))) {
		dt->day = 1;
		++dt->month;
	}
	if((dt->day >= 29 && dt->month == 2) && !(dt->year %400==0 ||(dt->year%4==0 && dt->year%100!=0))) {
		dt->day = 1;
		++dt->month;
	}
	if(dt->month >= 13) {
		dt->month = 1;
		++dt->year;
	}
	if(dt->month <= 0) {
		dt->month = 12;
		--dt->year;
	}
}

void advance_dt(DateTime *dt) {
	++dt->second;
	check_dt(dt);
}

void print_dt(const DateTime *dt) {
	char buf[17];
	// Print date on top row.
	lcd_pos(0, 0);
	sprintf(buf, "%02d-%02d-%04d", dt->month, dt->day, dt->year);
	lcd_puts2(buf);
	// Print time on bottom.
	lcd_pos(1, 0);
	sprintf(buf, "%02d:%02d:%02d", dt->hour, dt->minute, dt->second);
	lcd_puts2(buf);
}

void print_empty(int cursor, const DateTime *dt) {
	char buf[17];
	
	switch (cursor) {
		case 1:
			lcd_pos(0, 0);
			sprintf(buf, "%s-%02d-%04d", "__", dt->day, dt->year);
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%02d:%02d:%02d", dt->hour, dt->minute, dt->second);
			lcd_puts2(buf);
			break;
		case 2:
			lcd_pos(0, 0);
			sprintf(buf, "%02d-%s-%04d", dt->month, "__", dt->year);
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%02d:%02d:%02d", dt->hour, dt->minute, dt->second);
			lcd_puts2(buf);
			break;
		case 3:
			lcd_pos(0, 0);
			sprintf(buf, "%02d-%02d-%s", dt->month, dt->day, "____");
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%02d:%02d:%02d", dt->hour, dt->minute, dt->second);
			lcd_puts2(buf);
			break;
		case 4:
			lcd_pos(0, 0);
			sprintf(buf, "%02d-%02d-%04d", dt->month, dt->day, dt->year);
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%s:%02d:%02d", "__", dt->minute, dt->second);
			lcd_puts2(buf);
			break;
		case 5:
			lcd_pos(0, 0);
			sprintf(buf, "%02d-%02d-%04d", dt->month, dt->day, dt->year);
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%02d:%s:%02d", dt->hour, "__", dt->second);
			lcd_puts2(buf);
			break;
		case 6:
			lcd_pos(0, 0);
			sprintf(buf, "%02d-%02d-%04d", dt->month, dt->day, dt->year);
			lcd_puts2(buf);
			lcd_pos(1, 0);
			sprintf(buf, "%02d:%02d:%s", dt->hour, dt->minute, "__");
			lcd_puts2(buf);
			break;
		}
}

void increase(int cursor, DateTime *dt) {
	switch (cursor) {
		case 1:
			++dt->month;
			break;
		case 2:
			++dt->day;
			break;
		case 3:
			++dt->year;
			break;
		case 4:
			++dt->hour;
			break;
		case 5:
			++dt->minute;
			break;
		case 6:
			++dt->second;
			break;
	}
	check_dt(dt);
}

void decrease(int cursor, DateTime *dt) {
	switch (cursor) {
		case 1:
		--dt->month;
		break;
		case 2:
		--dt->day;
		break;
		case 3:
		--dt->year;
		break;
		case 4:
		--dt->hour;
		break;
		case 5:
		--dt->minute;
		break;
		case 6:
		--dt->second;
		break;
	}
	check_dt(dt);
}

int main() {
	DateTime dt;
	lcd_init();
	init_dt(&dt);
	while (1) 
	{
		int k;
		int cursor = 1;
		avr_wait(1000);
		k = get_key();
		// Enter setting mode if k is not 0
		if (k == 1) 
		{
			while (1) {
				k = get_key();
				if (k == 2)
				{
					break;
				}
				// Move cursor right
				else if (k == 4)
				{
					++cursor;
					if (cursor == 7) {
						cursor = 1;
					}
				}
				// Increase
				else if (k == 3) increase(cursor, &dt);
				// Decrease
				else if (k == 7) decrease(cursor, &dt);
				print_empty(cursor, &dt);
				avr_wait(200);
				print_dt(&dt);
				avr_wait(200);
			}
		}
		else {
			advance_dt(&dt);
			print_dt(&dt);
		}
	}
	return 0;
} 