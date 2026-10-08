#include <msp430.h>

#define TXLED BIT0
#define RXLED BIT6
#define TXD BIT2
#define RXD BIT1

void SendData(char *dat);
void Read_ADC(void);
void UART_OutUDec(unsigned int n);
int Temperature_value(int ADC_Value);
int temp_C = 0;
void Init_Motor_PWM(void);

char CRLF[] = {0x0a, 0x0d, 0x00};
unsigned int ADC_Value;

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD;
    
    int tmp;

    DCOCTL = 0; 
    BCSCTL1 = CALBC1_1MHZ; 
    DCOCTL = CALDCO_1MHZ;
    
    P2DIR |= 0xFF;
    P2OUT &= 0x00;
    
    P1SEL |= RXD + TXD;
    P1SEL2 |= RXD + TXD; 
    P1DIR |= RXLED + TXLED;
    P1OUT &= 0x00;
    
    UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 104;
    UCA0BR1 = 0x00; 
    UCA0MCTL = UCBRS2 + UCBRS0;
    UCA0CTL1 &= ~UCSWRST;

    // 1. INICIALIZAR EL MOTOR AQUÍ
    Init_Motor_PWM();
    
    while (1)
    {
        Read_ADC();
        tmp = Temperature_value(ADC_Value);

        UART_OutUDec(tmp);
        SendData(CRLF);

        // 2. LÓGICA DE CONTROL DE LA BOMBA DE AGUA
        if (tmp < 22) {
            TACCR1 = 0;      // Frío: Motor apagado (0%)
        }
        else if (tmp >= 22 && tmp < 26) {
            TACCR1 = 500;    // Templado: Motor a media potencia (50%)
        }
        else if (tmp >= 26) {
            TACCR1 = 1000;   // Caliente: Motor a máxima potencia (100%)
        }

        __delay_cycles(1000000);
    }
}

int Temperature_value(int ADC_Value){
    long Vout = (ADC_Value * 3300) / 1023; // Voltage out will be in mV
    int tmp = (Vout - 500) / 10; //Apply offset and divide 10mV/°C
    return tmp;
}

void SendData(char *dat)
{
    unsigned int j = 0;
    while(dat[j] != 0x00)
    {
        UCA0TXBUF = (dat[j]);
        __delay_cycles(1000); // SincronizaciÃ³n original del profesor
        j++;
    }
}

void Read_ADC(void)
{
    ADC10CTL0 = 0x00; // Stop ADC

    // Canal A3 (P1.3) y SMCLK
    ADC10CTL1 = INCH_3 + ADC10SSEL_3;
    // Habilitar entrada analÃ³gica en P1.3
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
        __delay_cycles(1000); // SincronizaciÃ³n original del profesor
        n = n % 10;
    }
    UCA0TXBUF = (n + 0x30);
}

void Init_Motor_PWM(void)
{
    // 1. Configurar los pines de dirección (AIN1 y AIN2)
    P2DIR |= BIT0 + BIT2;  // P2.0 y P2.2 como salidas
    P2OUT &= ~(BIT0 + BIT2); // Iniciar con el motor apagado (LOW)

    // 2. Configurar el pin de velocidad (PWMA) en P1.6
    P1DIR |= BIT6;         // P1.6 como salida
    P1SEL |= BIT6;         // P1.6 conectado al Timer_A (Módulo TA0.1)

    // 3. Configurar el Timer_A para generar la onda PWM
    // Periodo total de la onda (TACCR0) -> 1000 ciclos (1 kHz)
    TACCR0 = 1000 - 1;

    // Ciclo de trabajo inicial (TACCR1) -> 500 ciclos (50% de velocidad)
    TACCR1 = 500;

    // Modo de salida: Reset/Set (genera el pulso PWM)
    TACCTL1 = OUTMOD_7;

    // Encender el Timer: SMCLK (1MHz), Modo Up (cuenta hasta TACCR0)
    TACTL = TASSEL_2 + MC_1;
}
