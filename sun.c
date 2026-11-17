
/* 
 * File:   newmain.c
 * Author: jpman
 *
 * Created on August 18, 2026, 10:31 AM
 */

// CONFIG1L
#pragma config PLLDIV = 1       // PLL Prescaler Selection bits (No prescale (4 MHz oscillator input drives PLL directly))
#pragma config CPUDIV = OSC1_PLL2// System Clock Postscaler Selection bits ([Primary Oscillator Src: /1][96 MHz PLL Src: /2])
#pragma config USBDIV = 1       // USB Clock Selection bit (used in Full-Speed USB mode only; UCFG:FSEN = 1) (USB clock source comes directly from the primary oscillator block with no postscale)

// CONFIG1H
#pragma config FOSC = XT_XT        // Oscillator Selection bits (HS oscillator (HS))
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config IESO = OFF      // Oscillator Switchover mode disabled

// CONFIG2L
#pragma config PWRT = OFF       // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOR = ON         // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware only (SBOREN is disabled))
#pragma config BORV = 3         // Brown-out Reset Voltage bits (Minimum setting 2.05V)
#pragma config VREGEN = OFF     // USB Voltage Regulator Enable bit (USB voltage regulator disabled)

// CONFIG2H
#pragma config WDT = OFF        // Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)

// CONFIG3H
#pragma config CCP2MX = ON      // CCP2 MUX bit (CCP2 input/output is multiplexed with RC1)
#pragma config PBADEN = OFF     // PORTB A/D Enable bit (PORTB<4:0> pins are configured as analog input channels on Reset)
#pragma config LPT1OSC = OFF    // Low-Power Timer 1 Oscillator Enable bit (Timer1 configured for higher power operation)
#pragma config MCLRE = ON       // MCLR Pin Enable bit (MCLR pin enabled; RE3 input pin disabled)

// CONFIG4L
#pragma config STVREN = ON      // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF         // Single-Supply ICSP Enable bit (Single-Supply ICSP enabled)
#pragma config ICPRT = OFF      // Dedicated In-Circuit Debug/Programming Port (ICPORT Enable bit (ICPORT disabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled (Legacy mode))

// CONFIG5L
#pragma config CP0 = OFF
#pragma config CP1 = OFF
#pragma config CP2 = OFF
#pragma config CP3 = OFF

// CONFIG5H
#pragma config CPB = OFF
#pragma config CPD = OFF

// CONFIG6L
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF

// CONFIG6H
#pragma config WRTC = OFF
#pragma config WRTB = OFF
#pragma config WRTD = OFF

// CONFIG7L
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF

// CONFIG7H
#pragma config EBTRB = OFF

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>

#define _XTAL_FREQ 16000000

// LCD port pins
#define RS RB5
#define RW RB6
#define EN RB7
#define LED RB4

#define motor1 RC2
#define manual RA5

// Constant definitions
#define PWM_Frequency 8300 // PWM signal frequency
#define TMR2_Prescale 4 // Timer2 Prescaler selected for 4
#define Delay_PWM 100

unsigned int PDC10bit; // Global variable for duty cycle
volatile unsigned char manual_override = 0; // Global variable for manual override

// Commands for LCD display
#define ClrScreen 0x01
#define ReturnHome 0x02
#define DispONCurOFF 0x0C
#define FirstLine 0x80
#define SecondLine 0xC0
#define TwoLines57Mat 0x38

// Constants (delays)
#define Delay_LCD 20

// PORT definitions
#define LCD_Port PORTD
#define LCD_Tris TRISD

void Init_Ports(void)
{
    TRISBbits.TRISB0 = 1; // Start button
    TRISBbits.TRISB1 = 1; // Manual override button
    TRISBbits.TRISB4 = 0; // LED
    TRISBbits.TRISB5 = 0; // RS declared as output for LCD display
    TRISBbits.TRISB6 = 0; // RW declared as output for LCD display
    TRISBbits.TRISB7 = 0; // EN declared as output for LCD display

    TRISCbits.TRISC2 = 0; // Motor1 declared as output
    TRISAbits.TRISA5 = 1; // AN4 declared as analog input (manual override)

    LCD_Tris = 0; // All LCD pins declared as output
    LCD_Port = 0; // Initialize display PORT buffer to 0

    TRISAbits.TRISA2 = 1; // AN2 declared as analog input
}

// The next function writes a command to the LCD display
void Lcd_CmdWrite(unsigned char c){
    LCD_Port = c;
    RS = 0;
    RW = 0;
    EN = 1;
    __delay_ms(Delay_LCD);
    EN = 0;
}

// The next function writes data to the LCD display
void Lcd_DataWrite(unsigned char d){
    LCD_Port = d;
    RS = 1;
    RW = 0;
    EN = 1;
    __delay_ms(Delay_LCD);
    EN = 0;
}

// The next function writes a full text string to the LCD
void Message_LCD(unsigned char *s){
    while(*s){
        Lcd_DataWrite(*s++);
    }
}

// The next function gets the LCD set up
void Init_LCD(){
    Lcd_CmdWrite(TwoLines57Mat);
    Lcd_CmdWrite(DispONCurOFF);
    Lcd_CmdWrite(ClrScreen);
    Lcd_CmdWrite(FirstLine);
}

void Init_ADC_Module(){
    ADFM = 1; // A/D result right justified

    // AN2 channel is selected
    CHS3 = 0;
    CHS2 = 0;
    CHS1 = 1;
    CHS0 = 0;

    VCFG1 = 0; // VREF- 0V
    VCFG0 = 0; // VREF+ 5V

    PCFG3 = 1; // AN0-AN4 configured as analog
    PCFG2 = 0;
    PCFG1 = 1;
    PCFG0 = 0;

    ADCS2 = 0; // Configure the ADC clock
    ADCS1 = 1;
    ADCS0 = 0;

    ADON = 1; // Turn on the ADC converter
}

// Initialize PWM module
void Init_PWM(){
    P1M1 = 0; // Single PWM output
    P1M0 = 0;
    CCP1M3 = 1;
    CCP1M2 = 1;
    CCP1M1 = 0;
    CCP1M0 = 0;
    T2CKPS1 = 0; // Prescaler 1:4 for Timer 2
    T2CKPS0 = 1;
    TMR2ON = 1; // Turn timer 2 on
}

// Calculate duty cycle
void PWM_Duty(unsigned int duty){
    if(duty > 100){
        duty = 100;
    }

    PDC10bit =
    ((unsigned long)duty * 4 * (PR2 + 1))/100;

    DC1B1 = (PDC10bit >> 1) & 1;
    DC1B0 = PDC10bit & 1;

    CCPR1L = PDC10bit >> 2; // Store the remaining 8 bit
}

void PWM_Period(){  //wrote by the teaacher
    float PWMPeriod = 0.0;
    float Tosc = 0.0;

    PWMPeriod = 1.0/PWM_Frequency;
    Tosc = 1.0/_XTAL_FREQ;
    PR2 = (PWMPeriod/(4*Tosc*TMR2_Prescale))-1;
}

// The next function reads AN2 and displays the voltage
void Read_Analogue_Input(void) {
    char data[8];
    float ADC_read;
    unsigned int duty;

    if(manual_override == 1){
        CHS3 = 0; // AN4 channel is selected
        CHS2 = 1;
        CHS1 = 0;
        CHS0 = 0;
    }

    else{
        CHS3 = 0; // AN2 channel is selected
        CHS2 = 0;
        CHS1 = 1;
        CHS0 = 0;
    }

    GO_DONE = 1; // Starts an ADC conversion
    while(GO_DONE) {
    }

    ADC_read = ((ADRESH*256.0+ADRESL) * (5/1023.0)); //voltage
    duty = (unsigned int)((ADC_read/5.0)*100.0); // Map voltage to duty cycle from 0-100%

    // Calculate duty cycle
    PWM_Duty(duty);
    Lcd_CmdWrite(FirstLine);

    if(manual_override == 1){
        Message_LCD("Manual Override ");
    }

    else{
        sprintf(data,"%.2f",ADC_read);
        Message_LCD(data);
        Message_LCD(" V          ");
    }

    sprintf(data,"%u",duty);
    Lcd_CmdWrite(SecondLine);
    Message_LCD(data);
    Message_LCD(" %             ");

    __delay_ms(100);
}

void init_sequence(){
    Lcd_CmdWrite(FirstLine);
    Message_LCD("All goodie");
    Lcd_CmdWrite(SecondLine);
    Message_LCD("in the hoodie");
    __delay_ms(3000);
    Lcd_CmdWrite(ClrScreen);
    Lcd_CmdWrite(FirstLine);
    Message_LCD("Please");
    Lcd_CmdWrite(SecondLine);
    Message_LCD("stand by");
    LED = 1;
    __delay_ms(300);
    LED = 0;
    __delay_ms(300);
    LED = 1;
    __delay_ms(300);
    LED = 0;
    __delay_ms(300);
    LED = 1;
    __delay_ms(300);
    LED = 0;
    __delay_ms(300);
    LED = 1;
    Lcd_CmdWrite(ClrScreen);
}

void smart_delay(int time){
    int i;
    for(i = 0; i < time; i++){
        __delay_ms(1);
    }
}

void stand_by(){
    Lcd_CmdWrite(FirstLine);
    Message_LCD("All systems");
    Lcd_CmdWrite(SecondLine);
    Message_LCD("ready");
}

// Enable interrupts (INT0 and INT1)
void Enable_Interrupts() {
    TRISBbits.TRISB0 = 1;     // RB0 as input
    TRISBbits.TRISB1 = 1;     // RB1 as input

    INTEDG0 = 1;              // Rising edge: low to high
    INTEDG1 = 1;              // Rising edge: low to high
    INT0IF = 0;               // Clear INT0 flag
    INT1IF = 0;               // Clear INT1 flag
    INT0IE = 1;               // Enable RB0 / INT0 interrupt
    INT1IE = 1;               // Enable RB1 / INT1 interrupt
    GIE = 1;                  // Enable global interrupts
}

// Interrupt service routine
void __interrupt() Example_Interrupts(void) {

    if (INT1IF) {               // RB1 had a 0 to 1 transition
        manual_override = 1;   // Activate manual override
        INT1IF = 0;            // Clear the flag
    }

    if (INT0IF) {               // RB0 had a 0 to 1 transition
        manual_override = 0;     // Return to normal mode
        INT0IF = 0;              // Clear the flag
    }
}

void main(void) {
    Init_Ports();
    Init_LCD();
    Init_ADC_Module();
    init_sequence();
    stand_by();

    while(PORTBbits.RB0 == 0){
        if(PORTBbits.RB0 == 1){
            Lcd_CmdWrite(ClrScreen);
            break;
        }
    }

    Lcd_CmdWrite(ClrScreen);

    PWM_Period();
    PWM_Duty(0);
    Init_PWM();

    Enable_Interrupts();

    while(1) {
        Read_Analogue_Input();
    }
}
