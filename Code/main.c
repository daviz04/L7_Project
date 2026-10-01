#include <msp430.h>

#define TXLED BIT0
#define RXLED BIT6
#define TXD BIT2
#define RXD BIT1

void SendData(char *dat);
void Read_ADC(void);
void UART_OutUDec(unsigned int n);

char CRLF[] = {0x0a, 0x0d, 0x00}; // Salto de línea para Tera Term
unsigned int ADC_Value;

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Detener Watchdog Timer

    // Calibrar el reloj interno (DCO) exactamente a 1 MHz
    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    // Configurar pines de los LEDs
    P1DIR |= RXLED + TXLED;
    P1OUT &= ~(RXLED + TXLED);

    // Configurar pines de Hardware UART (P1.1 = RXD, P1.2 = TXD)
    P1SEL |= RXD + TXD;
    P1SEL2 |= RXD + TXD;

    // Configurar módulo USCI_A0 para UART a 9600 baudios
    UCA0CTL1 |= UCSSEL_2; // Usar SMCLK (1 MHz)
    UCA0BR0 = 104;        // 1MHz / 9600 = 104 (0x68)
    UCA0BR1 = 0x00;
    UCA0MCTL = UCBRS2 + UCBRS0; // Modulación (UCBRSx = 5)
    UCA0CTL1 &= ~UCSWRST; // Inicializar la máquina de estados USCI

    while (1)
    {
        Read_ADC();
        UART_OutUDec(ADC_Value); // Enviar el número
        SendData(CRLF);          // Enviar salto de línea (Enter)

        __delay_cycles(1000000); // Esperar 1 segundo
    }
}

void SendData(char *dat)
{
    unsigned int j = 0;
    while(dat[j] != 0x00)
    {
        while (!(IFG2 & UCA0TXIFG)); // Esperar a que el buffer esté listo
        UCA0TXBUF = dat[j];
        j++;
    }
}

void Read_ADC(void)
{
    ADC10CTL0 = 0x00; // Detener ADC

    // Seleccionar Canal A3 (P1.3) y reloj SMCLK
    ADC10CTL1 = INCH_3 + ADC10SSEL_3;
    ADC10AE0 = BIT3; // Habilitar entrada analógica en P1.3

    // Encender ADC y habilitar conversión
    ADC10CTL0 = ADC10ON + ENC;
    ADC10CTL0 |= ADC10SC; // Iniciar conversión

    while(ADC10CTL1 & ADC10BUSY)
    {
        // Esperar a que termine la conversión
    }

    ADC_Value = ADC10MEM; // Guardar el resultado numérico
}

void UART_OutUDec(unsigned int n)
{
    if(n >= 10)
    {
        UART_OutUDec(n / 10);
        n = n % 10;
    }
    while (!(IFG2 & UCA0TXIFG)); // Esperar a que el buffer esté listo
    UCA0TXBUF = (n + 0x30);      // Convertir a ASCII y enviar
}
