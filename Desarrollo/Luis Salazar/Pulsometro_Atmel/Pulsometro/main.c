/*
 * Pulsometro.c
 *
 * Created: 14/06/2020 12:20:09 p. m.
 * Author : Luis Gerardo
 */ 
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>		


#define	LCD_DPRT  PORTD	
#define	LCD_DDDR  DDRD		
#define	LCD_DPIN  PIND 		
#define	LCD_CPRT  PORTB 	
#define	LCD_CDDR  DDRB 		
#define	LCD_CPIN  PINB 		
#define	LCD_RS  0 			
#define	LCD_RW  1 			
#define	LCD_EN  2 			

//*******************************************************

//*******************************************************
void lcdCommand( unsigned char cmnd )
{
  LCD_DPRT = cmnd;			
  LCD_CPRT &= ~ (1<<LCD_RS);
  LCD_CPRT &= ~ (1<<LCD_RW);
  LCD_CPRT |= (1<<LCD_EN);	
  _delay_us(1);				
  LCD_CPRT &= ~ (1<<LCD_EN);
  _delay_us(100);			
}

//*******************************************************
void lcdData( unsigned char data )
{
  LCD_DPRT = data;			
  LCD_CPRT |= (1<<LCD_RS);	
  LCD_CPRT &= ~ (1<<LCD_RW);
  LCD_CPRT |= (1<<LCD_EN);	
  _delay_us(1);				
  LCD_CPRT &= ~ (1<<LCD_EN);
  _delay_us(100);			
}

//*******************************************************
void lcd_init()
{
  LCD_DDDR = 0xFF;
  LCD_CDDR = 0xFF;
 
  LCD_CPRT &=~(1<<LCD_EN);	
  _delay_us(2000);			
  lcdCommand(0x38);			
  lcdCommand(0x0E);			
  lcdCommand(0x01);			
  _delay_us(2000);			
  lcdCommand(0x06);			
}

//*******************************************************
void lcd_gotoxy(unsigned char x, unsigned char y)
{  
 unsigned char firstCharAdr[]={0x80,0xC0,0x94,0xD4};//table 12-5  
 lcdCommand(firstCharAdr[y-1] + x - 1);
 _delay_us(100);	
}

//*******************************************************
void lcd_print( char * str )
{
  unsigned char i = 0 ;
  while(str[i]!=0)
  {
    lcdData(str[i]);
    i++ ;
  }
}

//*******************************************************



///////////////////////////////////////////////////////
int PulsoPin = 0;

volatile int BPM;							// Pulsaciones por minuto
volatile int Signal;						// Entrada de datos del sensor de pulsos
volatile int IBI = 600;						// tiempo entre pulsaciones
volatile Pulse = 0;							// Verdadero cuando la onda de pulsos es alta, falso cuando es Baja		//Bool false
volatile QS = 0;							// Verdadero cuando el Arduino Busca un pulso del Corazon				//Bool false

volatile int rate[10];						// matriz para contener los últimos diez valores IBI
volatile unsigned long sampleCounter = 0;   // utilizado para determinar el tiempo de pulso
volatile unsigned long lastBeatTime = 0;    // usado para encontrar IBI
volatile int P =512;						// utilizado para encontrar el pico en la onda de pulso, sembrado
volatile int T = 512;						// utilizado para encontrar la depresión en la onda del pulso
volatile int thresh = 512;					// se utiliza encontrar momentos instantáneos de latidos cardíacos, sembrados
volatile int amp = 100;						// se utiliza para mantener la amplitud de la forma de onda del pulso, sin semillas
volatile firstBeat;							// se utiliza para sembrar la matriz de velocidad, así que comenzamos con BPM razonable //Bool true
volatile secondBeat = 0;					// se utiliza para sembrar la matriz de velocidad, así que comenzamos con BPM razonable			//Bool false

ISR(TIMER1_COMPA_vect)  {		//PORTB=PORTB|0X01;							//Pone Aa 1 el bit PB5 para encender led
	PulsoPin=ADCH;								//Lee el resultado de la conversión
	ADCSRA=0b11000111;							//Configuracion del ADC, preescalador de 128 para tener una frecuencia de 125kHz que es 8uS el tiempo de conversion
	UDR0=PulsoPin;								//Envia el valor obtenido por la UART
	//PORTB=PORTB&0XFE;							//Pone el bit PB5 en 0 pra apagar  el led		TCCR1A=0;									//Activacion del CTC como modo de operación	TCCR1B=0b00001100;							//Preescalador de 256 para la interrupción,	OCR1A=125;									//ESTABLECER LA PARTE SUPERIOR DEL CONTEO EN 124 PARA UNA TASA DE MUESTRA DE 500Hz	TIMSK1= 0b00000010;							//HABILITAR INTERRUPCIONES EN EL PARTIDO ENTRE TIMER1 Y OCR1A	}
////////////////////////////////////////////////

int main(void) 
{	
	lcd_init();
	lcd_gotoxy(1,1);
	lcd_print(" BPM");
	lcd_gotoxy(1,2);
	lcd_print(BPM);
	
	//////////////////////////////////////////////////////////////////////////////////////
	
	
	DDRB=0xFF|0b00001000;									//Todo el puerto B como salida
	DDRC=0;										//Todo el puerto C como entrada
		UCSR0B=0b00001000;							//Habilita la transmision por la UART
	UBRR0=103;									//Configurar velocidad a 9600 badios		TCCR1A=0;									//Activacion del CTC como modo de operación	TCCR1B=0b00001100;							//Preescalador de 256 para la interrupción, configuracion de timmer	OCR1A= 125;									//ESTABLECER LA PARTE SUPERIOR DEL CONTEO EN 124 PARA UNA TASA DE MUESTRA DE 500Hz	TIMSK1= 0b00000010;							//HABILITA INTERRUPCION DE COMPARACION A DEL TIMER 1	
	ADMUX= 0b11100000;							//SELECCIONA FUENTE DE AREF 1.1v, TIPO DE JUSTIFICACION, CANAL DE CONVERSION A0/BC0
	ADCSRA= 0b11000111;							//CONFIGURACIONES VARIAS AL ADC
	sei();										//HABILTA GLOBALMENTE LAS INTERRUPCIONES
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
	
	
	while(1){
		
		cli();									//Se activa cuando Timer2 cuenta hasta 124
		
		Signal = PINC|0X01;						//Lee el pulso del sensor
		sampleCounter += 2;                     //Realizar un seguimiento del tiempo en mS con esta variable
		int N = sampleCounter - lastBeatTime;   //controlar el tiempo transcurrido desde el último latido para evitar ruidos
		
		if (Signal < thresh && N > (IBI/5)*3){			//evite el ruido dicrótico al esperar 3/5 del último IBI
			if (Signal < T){							//T es el comedero
				T = Signal;							//realizar un seguimiento del punto más bajo en la onda de pulso
			}
		}
		
		if(Signal > thresh && Signal > P){          // la condición de trilla ayuda a evitar el ruido
			P = Signal;                             // P es el pico
		}											// realizar un seguimiento del punto más alto en la onda de pulso
		
		// AHORA ES HORA DE BUSCAR EL CORAZÓN
		// la señal aumenta de valor cada vez que hay un pulso
		
		if (N > 250){															// evitar el ruido de alta frecuencia //cambie false por 0
			if
			( (Signal > thresh) && (Pulse == 0) && (N > (IBI/5)*3) ){
				Pulse = 0;															// establecer la bandera de pulso cuando creemos que hay un pulso   //cambie false por 0
				
				IBI = sampleCounter - lastBeatTime;								// medir el tiempo entre latidos en mS
				lastBeatTime = sampleCounter;										// llevar un registro del tiempo para el próximo pulso
				
				if(secondBeat){													// si este es el segundo tiempo, si secondBeat == TRUE
					secondBeat = 0;												// limpia segunda bandera de Beat  //cambie false por 0
					for(int i=0; i<=9; i++){										// sembrar el total acumulado para obtener un BPM realista al inicio
						rate[i] = IBI;
					}
				}
				
				if(firstBeat){														// si es la primera vez que encontramos un ritmo, si firstBeat == TRUE
					firstBeat = 0;													// borrar primero					//cambie false por 0
					secondBeat = 1;												// establecer la segunda bandera de ritmo
					sei();															// Hanilitar interrupciones nuevamente
					return(0);														// El valor de IBI no es confiable, así que deséchelo
				}
				
				// mantener un total acumulado de los últimos 10 valores de IBI
				int runningTotal = 0;						// clear the runningTotal variable					//Tenia world //Cambie de word a int

				for(int i=0; i<=8; i++){					// borrar la variable Total en ejecución
					rate[i] = rate[i+1];                  // y soltar el valor IBI más antiguo
					runningTotal += rate[i];              // sumar los 9 valores IBI más antiguos
				}

				rate[9] = IBI;                          // agregar el último IBI a la matriz de tarifas
				runningTotal += rate[9];                // agregue el último IBI para ejecutar Total
				runningTotal /= 10;                     // promediar los últimos 10 valores de IBI
				BPM = 60000/runningTotal;               // ¿Cuántos latidos pueden caber en un minuto? eso es BPM!
				QS = 1;                              // establecer el indicador de auto cuantificado	//CAMBIAR TRUE POR 1
				
				// LA BANDERA QS NO SE LIMPIA DENTRO DE ESTE ISR
			}
		}
		
		if (Signal < thresh && Pulse == 1){   // cuando los valores bajan, el ritmo termina			//SE CAMBIA EL TRUE POR 1

			Pulse = 0;								// cuando los valores están bajando, el ritmo ha terminado			//Cambie un false por 0
			amp = P - T;							// obtener amplitud de la onda de pulso
			thresh = amp/2 + T;						// establecer la trilla al 50% de la amplitud
			P = thresh;								// restablecer estos para la próxima vez
			T = thresh;
		}

		if (N > 2500){								// si pasan 2.5 segundos sin latir
			thresh = 512;                          // establecer umbral predeterminado
			P = 512;                               // establecer P por defecto
			T = 512;                               // establecer T por defecto
			lastBeatTime = sampleCounter;          // actualizar lastBeatTime
			firstBeat = 1;                      // configurar estos para evitar el ruido			//SE CAMBIA EL TRUE POR 1
			secondBeat = 0;                    // cuando recuperamos el latido del corazón			//SE CAMBIA EL FALSE POR 0
		}

		sei();                                   // ¡habilita las interrupciones cuando hayas terminado!

		int pulso = (PINC|0X01);					//Lee el valor del pulsometro conectado al puerto Analogo A0
		if (pulso >= 520) {						// Enciende led 13 cuando el pulso pasa de un valor (debe ajustarse)
			PORTB==(PORTC|0Xb00001000);					//PONE A 1 EL BIT PB5 (PIN 13 DE LA TARJETA ARDUINO UNO)
		}
		else{
			PORTB=PORTC&0X11110111;					//PONE A 0 EL BIT PB5 (PIN 13 DE LA TARJETA ARDUINO UNO)
		}
	}				
    	return 0;
}

