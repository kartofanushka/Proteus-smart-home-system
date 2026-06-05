#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h> // Interrupt support

/* ================= LCD ============== */
#define LCD_DATA_PORT PORTB
#define LCD_DATA_DDR  DDRB
#define LCD_CTRL_PORT PORTA
#define LCD_CTRL_DDR  DDRA
#define RS PA5
#define RW PA6
#define E  PA7

void lcd_enable() {
	LCD_CTRL_PORT |= (1 << E);
	_delay_us(1);
	LCD_CTRL_PORT &= ~(1 << E);
	_delay_ms(2);
}

void lcd_send(uint8_t data, uint8_t rs) {
	if (rs) LCD_CTRL_PORT |= (1 << RS);
	else    LCD_CTRL_PORT &= ~(1 << RS);
	LCD_CTRL_PORT &= ~(1 << RW);
	LCD_DATA_PORT = data;
	lcd_enable();
}

void lcd_cmd(uint8_t cmd) { lcd_send(cmd, 0); }
void lcd_data(uint8_t data) { lcd_send(data, 1); }

void lcd_init() {
	LCD_DATA_DDR = 0xFF;
	LCD_CTRL_DDR |= (1 << RS) | (1 << RW) | (1 << E);
	_delay_ms(20);
	lcd_cmd(0x38);
	lcd_cmd(0x0C);
	lcd_cmd(0x06);
	lcd_cmd(0x01);
}

void lcd_print(const char *str) {
	while (*str) lcd_data(*str++);
}

/* ================= UART ================= */
void uart_init() {
	// Set baud rate to 9600 for 8MHz system clock
	UBRRH = 0;
	UBRRL = 51;
	
	// Enable transmitter
	UCSRB = (1 << TXEN);
	
	// Frame format: 8 data bits, 1 stop bit
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void uart_transmit(char data) {
	while (!(UCSRA & (1 << UDRE))); // Wait for empty transmit buffer
	UDR = data;
}

void uart_print(const char *str) {
	while (*str) uart_transmit(*str++);
}

void uart_print_num(uint16_t num) {
	if (num == 0) {
		uart_transmit('0');
		return;
	}
	
	char buf[6];
	uint8_t i = 0;
	
	while (num > 0) {
		buf[i++] = (num % 10) + '0';
		num /= 10;
	}
	
	while (i > 0) {
		uart_transmit(buf[--i]);
	}
}

/* ================= KEYPAD =============== */
#define KP_PORT PORTC
#define KP_PIN  PINC
#define KP_DDR  DDRC

const char keymap[4][4] = {
	{'7','8','9','/'},
	{'4','5','6','*'},
	{'1','2','3','-'},
	{'C','0','=','+'}
};

void keypad_init() {
	KP_DDR = 0x0F;
	KP_PORT = 0xF0;
}

char keypad_getkey() {
	for (uint8_t col = 0; col < 4; col++) {
		KP_PORT |= 0x0F;
		KP_PORT &= ~(1 << col);
		_delay_us(10);
		uint8_t row = KP_PIN & 0xF0;
		if (row != 0xF0) {
			_delay_ms(5);
			row = KP_PIN & 0xF0;
			if (!(row & (1 << PC4))) return keymap[0][col];
			if (!(row & (1 << PC5))) return keymap[1][col];
			if (!(row & (1 << PC6))) return keymap[2][col];
			if (!(row & (1 << PC7))) return keymap[3][col];
		}
	}
	return 0;
}

/* ================= IO, ADC & INTERRUPTS =============== */
#define BUZZER PA1
#define LED_BLUE PA3
#define LED_PINK PA4
#define FAN PD4

void io_init() {
	DDRA |= (1 << BUZZER) | (1 << LED_BLUE) | (1 << LED_PINK);
	DDRD |= (1 << FAN);
}

void adc_init() {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // Prescaler 64
}

uint16_t get_temperature() {
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return (ADC * 500UL) / 1024;
}

void interrupt_init() {
	DDRD &= ~(1 << PD2); // Set PD2 (INT0) as input
	PORTD |= (1 << PD2); // Enable pull-up resistor on PD2

	// Trigger INT0 on falling edge
	MCUCR |= (1 << ISC01);
	MCUCR &= ~(1 << ISC00);
	
	GICR |= (1 << INT0); // Enable INT0
	sei();               // Enable global interrupts
}

/* ================= LOGIC & STATES =============== */
typedef enum { STATE_LOGIN, STATE_MENU, STATE_VIEW, STATE_SET, STATE_EMERGENCY } SystemState;
volatile SystemState state = STATE_LOGIN; // volatile is required as it is modified in ISR

uint16_t threshold = 28;
char correct_pin[4] = {'1','2','3','4'};
char entered_pin[4];
uint8_t pin_index = 0;
char input_buf[3];
uint8_t input_i = 0;

/* ===== EMERGENCY BUTTON ISR ===== */
ISR(INT0_vect) {
	state = STATE_EMERGENCY;
	
	PORTD &= ~(1 << FAN);                     // Disable fan
	PORTA |= (1 << BUZZER) | (1 << LED_PINK); // Enable alarms
	PORTA &= ~(1 << LED_BLUE);                // Disable normal operation LED
}

/* ===== LCD VIEWS ===== */
void show_login() {
	lcd_cmd(0x01);
	lcd_print("ANGELA ABOL");
	lcd_cmd(0xC0);
	lcd_print("Pin:");
}

void show_menu() {
	lcd_cmd(0x01);
	lcd_print("1:Temp 2:Set");
}

/* ===== HANDLE FUNCTIONS ===== */
void handle_login() {
	char key = keypad_getkey();
	if (key) {
		if (key == 'C') {
			pin_index = 0;
			show_login();
		} else if (key >= '0' && key <= '9' && pin_index < 4) {
			entered_pin[pin_index++] = key;
			lcd_data('*');
		}

		if (pin_index == 4) {
			_delay_ms(50);
			lcd_cmd(0x
