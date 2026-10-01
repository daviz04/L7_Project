#include <msp430.h>

#define TXLED BIT0
#define RXLED BIT6
#define TXD BIT2
#define RXD BIT1

void SendData(char *dat);
void Read_ADC(void);
void UART_OutUDec(unsigned int n);

char CRLF[] = {0x0a, 0x0d, 0x00};
unsigned int ADC_Value;

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Stop WDT
    
    // Calibración DCO a 1 MHz
    DCOCTL = 0; 
    BCSCTL1 = CALBC1_1MHZ; 
    DCOCTL = CALDCO_1MHZ;
    
    P2DIR |= 0xFF; // All P2.x outputs
    P2OUT &= 0x00; // All P2.x reset
    
    P1SEL |= RXD + TXD; // P1.1 = RXD, P1.2=TXD
    P1SEL2 |= RXD + TXD; 
    P1DIR |= RXLED + TXLED;
    P1OUT &= 0x00;
    
    UCA0CTL1 |= UCSSEL_2; // SMCLK
    UCA0BR0 = 104; // 1MHz / 9600
    UCA0BR1 = 0x00; 
    UCA0MCTL = UCBRS2 + UCBRS0; // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST; // Initialize USCI state machine
    
    while (1)
    {
        Read_ADC();
        UART_OutUDec(ADC_Value);
        SendData(CRLF);
        
        __delay_cycles(1000000);
    }
}

void SendData(char *dat)
{
    unsigned int j = 0;
    while(dat[j] != 0x00)
    {
        UCA0TXBUF = (dat[j]);
        __delay_cycles(1000); // Sincronización original del profesor
        j++;
    }
}

void Read_ADC(void)
{
    ADC10CTL0 = 0x00; // Stop ADC
    
    // Canal A3 (P1.3) y SMCLK
    ADC10CTL1 = INCH_3 + ADC10SSEL_3; 
    // Habilitar entrada analógica en P1.3
    ADC10AE0 = BIT3; 
    
    ADC10CTL0 = ADC10ON + ENC; // Turn on ADC and Enable Conversion
    ADC10CTL0 |= ADC10SC; // Start conversion

    while(ADC10CTL1 & ADC10BUSY) // Wait until conversion is complete
    {
    }

    ADC_Value = ADC10MEM; // Read result
}

void UART_OutUDec(unsigned int n)
{
    if(n >= 10)
    {
        UART_OutUDec(n / 10);
        __delay_cycles(1000); // Sincronización original del profesor
        n = n % 10;
    }
    UCA0TXBUF = (n + 0x30);
}