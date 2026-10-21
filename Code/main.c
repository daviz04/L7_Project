#include <msp430.h>

#define TXLED BIT0
#define RXLED BIT6
#define TXD BIT2
#define RXD BIT1

void SendData(char *dat);
unsigned int Read_ADC(unsigned int canal);
void UART_OutUDec(unsigned int n);
int Temperature_value(int ADC_Value);
int Humidity_value(unsigned int ADC_Value);
void Init_Motor_PWM(void);
int temp_C = 0;

char CRLF[] = { 0x0a, 0x0d, 0x00 };
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
        // 1. Leer Temperatura (Pin P1.3) ---
        unsigned int adc_temp = Read_ADC(3);
        tmp = Temperature_value(adc_temp);

        // 2. Leer Humedad (Pin P1.4) ---
        unsigned int adc_hum = Read_ADC(4);
        int hum = Humidity_value(adc_hum);

        // 3. Enviar todo a Tera Term con formato ---
        SendData("Temp: ");
        UART_OutUDec(tmp);
        SendData(" C   Humedad: ");
        UART_OutUDec(hum);
        SendData("%");
        SendData(CRLF); // Salto de línea

        // 4. Control del motor (se mantiene igual) ---
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

        __delay_cycles(1000000); // Esperar un segundo
    }
}

int Temperature_value(int ADC_Value)
{
    long Vout = (ADC_Value * 3300) / 1023; // Voltage out will be in mV
    int tmp = (Vout - 500) / 10; //Apply offset and divide 10mV/°C
    return tmp;
}

void SendData(char *dat)
{
    unsigned int j = 0;
    while (dat[j] != 0x00)
    {
        // Esperar activamente a que el búfer de transmisión esté libre
        while (!(IFG2 & UCA0TXIFG));

        UCA0TXBUF = (dat[j]);
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

    // Esperar activamente a que el búfer de transmisión esté libre
    while (!(IFG2 & UCA0TXIFG));

    UCA0TXBUF = (n + 0x30);
}

unsigned int Read_ADC(unsigned int canal)
{
    ADC10CTL0 &= ~ENC; // Apagar la conversión temporalmente para cambiar de pin

    if (canal == 3)
    {
        ADC10CTL1 = INCH_3 + ADC10SSEL_3; // Configurar para el canal A3 (Pin 1.3 - Temp)
        ADC10AE0 |= BIT3;
    }
    else if (canal == 4)
    {
        ADC10CTL1 = INCH_4 + ADC10SSEL_3; // Configurar para el canal A4 (Pin 1.4 - Humedad)
        ADC10AE0 |= BIT4;
    }

    ADC10CTL0 = ADC10ON + ENC; // Volver a encender
    ADC10CTL0 |= ADC10SC;      // Iniciar conversión

    while (ADC10CTL1 & ADC10BUSY)
    {
        // Esperar a que termine de leer el voltaje
    }

    return ADC10MEM; // Devolver el resultado bruto
}

// Function to read humidity in human values
int Humidity_value(unsigned int ADC_Value)
{
    // Fórmula optimizada sin decimales basada en el datasheet:
    int rh = ((ADC_Value - 155) * 10) / 65;

    // Límites de seguridad por si hay ruido eléctrico al arrancar
    if (rh < 0)
        rh = 0;
    if (rh > 100)
        rh = 100;

    return rh;
}

void Init_Motor_PWM(void)
{
    // 1. Configure direction pins (AIN1 y AIN2)
    P2DIR |= BIT0 + BIT2;  // P2.0 y P2.2 as outputs
    P2OUT &= ~(BIT0 + BIT2); // Start with the motor off (LOW)

    // 2. Configure el speed pin (PWMA) in P1.6
    P1DIR |= BIT6;         // P1.6 as output
    P1SEL |= BIT6;         // P1.6 connected to Timer_A (Módulo TA0.1)

    // 3. Configure Timer_A to generate PWM wave
    // Periodo total de la onda (TACCR0) -> 1000 ciclos (1 kHz)
    TACCR0 = 1000 - 1;

    // Ciclo de trabajo inicial (TACCR1) -> 500 ciclos (50% de velocidad)
    TACCR1 = 500;

    // Modo de salida: Reset/Set (genera el pulso PWM)
    TACCTL1 = OUTMOD_7;

    // Encender el Timer: SMCLK (1MHz), Modo Up (cuenta hasta TACCR0)
    TACTL = TASSEL_2 + MC_1;
}
