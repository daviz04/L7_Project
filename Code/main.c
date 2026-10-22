#include <msp430.h>

#define TXLED BIT0
#define RXLED BIT6
#define TXD BIT2
#define RXD BIT1

void SendData(char *dat);
unsigned int Read_ADC(unsigned int canal);
void UART_OutUDec(unsigned int n);
int Temperature_value(unsigned int ADC_Value);
int Humidity_value(unsigned int ADC_Value);
void Init_Motor_PWM(void);

char CRLF[] = { 0x0d, 0x0a, 0x00 }; // Standard CR + LF for Tera Term

int main(void)
{
    // 1. Stop Watchdog Timer
    WDTCTL = WDTPW + WDTHOLD;

    // 2. Check and load 1MHz factory calibration
    if (CALBC1_1MHZ == 0xFF)
    {
        while(1); // Trap CPU if calibration constants are erased
    }
    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    // 3. Configure Ports
    P2DIR |= 0xFF;
    P2OUT = 0x00;

    P1DIR |= TXLED;       // P1.0 (Red LED) as output
    P1OUT &= ~TXLED;      // Start with Red LED OFF

    // 4. Configure UART on P1.1 (RXD) and P1.2 (TXD) - STRICT USCI SEQUENCE
    UCA0CTL1 = UCSWRST;             // **CRITICAL: Put USCI in reset BEFORE configuring**
    P1SEL |= RXD + TXD;             // Connect P1.1 and P1.2 to USCI_A0 UART
    P1SEL2 |= RXD + TXD;
    UCA0CTL1 |= UCSSEL_2;           // Clock source: SMCLK (1 MHz)
    UCA0BR0 = 104;                  // 1MHz / 9600 = 104
    UCA0BR1 = 0x00;
    UCA0MCTL = UCBRS0;              // Modulation UCBRSx = 1 (standard for 1MHz 9600)
    UCA0CTL1 &= ~UCSWRST;           // **Release USCI reset to start UART**

    // 5. Initiate the motor PWM on P1.6
    Init_Motor_PWM();

    // Send startup message to confirm UART works immediately
    SendData("MSP430 System Initialized...");
    SendData(CRLF);

    while (1)
    {
        // 1. Read sensors on P1.3, P1.4 and P1.5
        unsigned int adc_temp = Read_ADC(3);
        int tmp = Temperature_value(adc_temp);

        unsigned int adc_hum = Read_ADC(4);
        int hum = Humidity_value(adc_hum);

        unsigned int adc_leak = Read_ADC(5); // 0 when dry, >200 when wet

        // 2. Send data to Tera Term
        SendData("Temp: ");
        UART_OutUDec((unsigned int)tmp);
        SendData(" C   Humidity: ");
        UART_OutUDec((unsigned int)hum);
        SendData("%   Leak ADC: ");
        UART_OutUDec(adc_leak);

        // 3. Priority check: Water leak emergency shutoff!
        if (adc_leak > 150)
        {
            TACCR1 = 0;               // Force pump OFF immediately
            P1OUT |= BIT0;            // Keep Red LED (P1.0) ON solid as visual alarm
            SendData(" [ALERT: LEAK DETECTED - PUMP OFF]");
        }
        else
        {
            // Blink Red LED briefly to show the loop is alive and transmitting
            P1OUT |= BIT0;
            __delay_cycles(50000);
            P1OUT &= ~BIT0;

            SendData(" [STATUS: OK]");

            // Normal temperature PWM control
            if (tmp < 22)
            {
                TACCR1 = 0;
            }
            else if (tmp >= 22 && tmp < 26)
            {
                TACCR1 = 500;
            }
            else if (tmp >= 26)
            {
                TACCR1 = 1000;
            }
        }

        SendData(CRLF);          // New line
        __delay_cycles(950000);  // Wait ~1 sec
    }
}

int Temperature_value(unsigned int ADC_Value)
{
    // Cast to 32-bit long BEFORE multiplying by 3300 to prevent 16-bit overflow!
    long Vout = ((long)ADC_Value * 3300L) / 1023L;
    long tmp = (Vout - 500L) / 10L;

    if (tmp < 0)
        tmp = 0; // Prevent negative numbers in unsigned print function

    return (int)tmp;
}

int Humidity_value(unsigned int ADC_Value)
{
    // Cast to signed 32-bit long so values below 155 do not underflow!
    long rh = (((long)ADC_Value - 155L) * 10L) / 65L;

    if (rh < 0)
        rh = 0;
    if (rh > 100)
        rh = 100;

    return (int)rh;
}

void SendData(char *dat)
{
    unsigned int j = 0;
    while (dat[j] != 0x00)
    {
        while (!(IFG2 & UCA0TXIFG));
        UCA0TXBUF = dat[j];
        j++;
    }
}

void UART_OutUDec(unsigned int n)
{
    if (n >= 10)
    {
        UART_OutUDec(n / 10);
        n = n % 10;
    }
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = (n + 0x30);
}

unsigned int Read_ADC(unsigned int canal)
{
    ADC10CTL0 &= ~ENC; // Disable ADC before changing channel

    if (canal == 3)
    {
        ADC10CTL1 = INCH_3 + ADC10SSEL_3; // Pin P1.3 - Temperature
        ADC10AE0 = BIT3;                  // Assign ONLY BIT3
    }
    else if (canal == 4)
    {
        ADC10CTL1 = INCH_4 + ADC10SSEL_3; // Pin P1.4 - Humidity
        ADC10AE0 = BIT4;                  // Assign ONLY BIT4
    }
    else if (canal == 5)
    {
        ADC10CTL1 = INCH_5 + ADC10SSEL_3; // Pin P1.5 - Water Leak Probe
        ADC10AE0 = BIT5;                  // Assign ONLY BIT5
    }

    ADC10CTL0 = SREF_0 + ADC10SHT_2 + ADC10ON + ENC; // 16 clock cycles + Turn on
    ADC10CTL0 |= ADC10SC;                            // Start conversion
    while (ADC10CTL1 & ADC10BUSY);                   // Wait until conversion finishes

    return ADC10MEM;
}

void Init_Motor_PWM(void)
{
    // Configure direction pins (AIN1 and AIN2)
    P2DIR |= BIT0 + BIT2;
    P2OUT |= BIT0;        // P2.0 HIGH (DIR1 forward)
    P2OUT &= ~BIT2;       // P2.2 LOW

    // Configure speed pin (PWMA) on P1.6
    P1DIR |= BIT6;
    P1SEL |= BIT6;        // P1.6 connected to Timer_A (TA0.1)
    P1SEL2 &= ~BIT6;      // Ensure P1SEL2 bit 6 is 0 for Timer_A output

    // Configure Timer_A to generate 1 kHz PWM wave
    TACCR0 = 1000 - 1;
    TACCR1 = 500;
    TACCTL1 = OUTMOD_7;
    TACTL = TASSEL_2 + MC_1;
}
