#include "lpc17xx.h"
#include <stdio.h>
#include "math.h"

#define C0 0x3F
#define C1 0x30
#define C2 0x5B
#define C3 0x4F
#define C4 0x66
#define C5 0x6D
#define C6 0x7D
#define C7 0x07 // (0b111_1000)
#define C8 0x7F // (0b000_0000)
#define C9 0x6F // (0b001_1000)
#define CA 0x77 // (0b0001000)
#define CB 0x7F // (0b0000011)
#define CC 0x39 // (0b1000110)
#define CD 0x21 // (0b0100001)
#define CE 0x79 // (0b0000110)
#define CF 0x71 // (0b0001110)
#define CH 0x76 // (0b0001001)
#define CO 0x3F // (0b1000000)
#define CL 0x38 // (0b1000111)
#define CR 0x2F // (0b0101111)
#define CG 0x3D // (0b0000010)
#define CN 0x2D // (0b0101011)
#define Cr 0x50
#define C__ 0x5F // (0b1011111)
#define Ce 0x7B
#define Cn 0x54
#define Cline 0x40
#define SystemFrequency 100000000
#define Num_Muestras 50
#define Pi 3.14159
#define DAC_BIAS	0x00010000  // Settling time a valor 2,5us
#define sw_frecuencimetro (1<<0)
#define sw_generador (1<<1) 
#define sw_visualizar (1<<2)
#define led_frecuencimetro (1<<3) 
#define led_generador (1<<4) 
#define led_seno (1<<5) 
#define led_triangulo (1<<6) 
#define led_sierra (1<<7) 
#define sw_tipofuncion0 (1<<8)
#define sw_tipofuncion1 (1<<9)
#define puls_configurar (1<<10)
#define puls_mas (1<<11)
#define puls_menos (1<<12)
#define frec_input (1<<13)

//Array Principal
uint16_t visual[4]={0,0,0,0}; //La función de este array es almacenar los datos para visualizar en el display

//Array Funciones
uint32_t FuncionSin[Num_Muestras];
uint32_t FuncionTri[Num_Muestras];
uint32_t FuncionSierra[Num_Muestras];
uint32_t Zero[Num_Muestras];

//Arrays para visualizacion en el display
const uint8_t segment[10]={C0,C1,C2,C3,C4,C5,C6,C7,C8,C9}; //Valores del 0-9
const uint8_t HOLA[4]={CH,C0,CL,CA}; //Array para visualizar HOLA
const uint8_t Gen[4]={CG,Ce,Cn,Cline};//Array para visualizar Gen_
const uint8_t frec[4]={CF,Cr,CE,CC};//Array para mostrar FrEC
const uint8_t HHHH[4]={CH,CH,CH,CH};//Array para mostrar HHHH

//Variables sueltas empleadas a lo largo del programa
uint8_t i,a,seleccion=0,aux1,aux2; //Variables utilizadas para los IFs y los FOR

 //Variables para el generador+Array generador
uint32_t cg[4]={0,1,0,0}; //Array generador. Almacena los valores del generador
uint32_t cont; //Variable donde voy a contabilizar las muestras de las funciones
uint32_t frecuencia=100; //Frecuencia Generador, de inicio es 100Hz

//Variables para el frecuencimetro+Array frecuencimetro
uint32_t cf[4]={0,0,0,0}; //Array frecuencias. Array donde se guardaran los datos asignados para la frecuencia
uint32_t FrecuenciaKHz; //Frecuencia pulso. Almaceno el valor obtenido, que no será otro que 2*numero_de_flancos 
uint32_t tick=100; //Variable que utilizo en el SysTick para contabilizar los tiempos
uint32_t numero_de_flancos=0; //Variable donde contabilizaré los flancos ascendentes

//Variables para las ISRs
uint32_t flagext0=0; //Variable Configuracion
uint32_t flagext1=0; //Variable pulso mas
uint32_t flagext2=0; //Variable pulso menos

void Funciones(){ //Con esta funcion determino las funciones de inicio. De primeras representaran la funcion a 100Hz
	for(i=0;i<Num_Muestras;i++){
	FuncionSin[i]=512+512*sin(2*Pi*i/Num_Muestras); 
	}
	for(i=0;i<Num_Muestras;i++){
		if(i<25){ //Parte del triángulo ascendente
			FuncionTri[i]=(2046*i)/(Num_Muestras);
		}else{ //Parte del triángulo descendente
			FuncionTri[i]=(2046*(i-a))/(Num_Muestras);
			a++;
			if(a>=25){
			 a=0;
			}
		}
	}
	for(i=0;i<Num_Muestras;i++){
		FuncionSierra[i]=(1023*i)/(Num_Muestras);
	}
	for(i=0;i<Num_Muestras;i++){
		Zero[i]=0;
	}
}
void configGPIO(void){ //Función para configurar todos los pines
	LPC_PINCON->PINSEL0=0;//Configuramos los pines P0.[0..15] como GPIO
	LPC_PINCON->PINSEL3=0;//Configuramos los pines P1.[16..31] como GPIO
	LPC_PINCON->PINSEL4=0; //Configuramos los pines P2.[0..15] como GPIO
	LPC_GPIO0->FIODIR=0x000000F0; //Pines P0.[4..7] como salidas
	LPC_GPIO1->FIODIR=0x0FF00000; //Pines P1.[20..27] como salidas
	LPC_GPIO0->FIODIR &=~ sw_tipofuncion0; //Pin P0.8 definido como entrada, sw_tipo_funcion "0"
	LPC_GPIO0->FIODIR &=~ sw_tipofuncion1; //Pin P0.9 definido como entrada, sw_tipo_funcion "1"
	LPC_GPIO2->FIODIR &=~ sw_frecuencimetro; //Pines P2.0, P2.1, P2.2, P2.10, P2.11, P2.12 y P2.13 configurados como entradas 
	LPC_GPIO2->FIODIR &=~ sw_generador;
	LPC_GPIO2->FIODIR &=~ sw_visualizar;
	LPC_GPIO2->FIODIR &=~ puls_configurar;
	LPC_GPIO2->FIODIR &=~ puls_mas;
	LPC_GPIO2->FIODIR &=~ puls_menos;
	LPC_GPIO2->FIODIR &=~ frec_input;
	LPC_GPIO2->FIODIR |= led_frecuencimetro|led_generador|led_seno|led_triangulo|led_sierra; //Pines P2.[3..7] configurados como salidas 
	LPC_GPIO0->FIOPIN = 0xF0; //Inicializo todos los displays como apagados
	LPC_GPIO2->FIOPIN |= led_frecuencimetro|led_generador|led_seno|led_triangulo|led_sierra; //Apago los LEDs de los P2.[3..7] de inicio
}
void ConfigDAC(void){
  // Configuramos el pin p0.26 como salida DAC 
	// En PINSEL1 bits 20 a 0 y bit 21 a 1
  LPC_PINCON->PINSEL1&=~(0x1<<20);
	LPC_PINCON->PINSEL1|=(0x1<<21);
	LPC_PINCON->PINMODE1&=~((3<<20));
	LPC_PINCON->PINMODE1|=(2<<20);		//Pin 0.26 funcionando sin pull-up/down	
  return;
}
void Configura_SysTick(void){
	SysTick->LOAD=499999; //Configuramos para que se introduzca interrupcion cada 5ms
	SysTick->VAL=0; //Valor inicial=0
	SysTick->CTRL|=0x7;//Activamos el reloj y el tickin
}
void ConfiguraTimer0(void){//Configuración del Timer0 para el generador
  LPC_SC->PCLKSEL0|=(1<<3); //Configuro el CCLK/2 a 50MHz-> 20ns
  LPC_TIM0->MR0=(1000000/frecuencia)-1;//Configuro la interrupción por match del Timer0 de inicio
	LPC_TIM0->PR=0;//Cada vez que el Prescaler alcance este valor se incrementa en uno el contador del Timer0, es decir, cada 10e-7 s
	LPC_TIM0->MCR=(1<<0)|(1<<1);//Habilito la interrupción por match y el reseteo
	LPC_TIM0->TCR |= 1<<1;//Resetear Timer 
	LPC_TIM0->TCR &=~(1<<1); // Volvemos a poner a 0 para eliminar el Reset de la línea anterior
	NVIC_EnableIRQ(TIMER0_IRQn); // Habilitar interrupción Timer0
	LPC_TIM0->TCR |= 1 << 0; // Arrancar el Timer 
}
void ConfiguracionEXT(void){
	LPC_PINCON->PINSEL4|=(0x01<<20); //Configuro P2.10 como entrada de interrupcion EXT0
	LPC_PINCON->PINSEL4|=(0x01<<22); //Configuro P2.11 como entrada de interrupcion EXT1
	LPC_PINCON->PINSEL4|=(0x01<<24); //Configuro P2.12 como entrada de interrupcion EXT2
	LPC_PINCON->PINSEL4&=~(1<<2*13); //Configuro P2.13 "10" como EXT3 - bit 0  en 2*13
	LPC_PINCON->PINSEL4|=(1<<2*13+1); //Configuro P2.13 "10" como EXT3 - bit 1 en 2*13+1
	LPC_SC->EXTMODE|=(0<<0); //EXT0 activa por sensibilidad de nivel, en este caso nivel de subida (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<0); //EXT0 activa a nivel bajo
	LPC_SC->EXTMODE|=(0<<1); //EXT1 activa por sensibilidad de nivel (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<1); //EXT1 activa a nivel bajo
	LPC_SC->EXTMODE|=(0<<2); //EXT2 activa por sensibilidad de nivel (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<2); //EXT2 activa a nivel bajo
	LPC_SC->EXTMODE|=(1<<3); //Interrupcion EXT3 activa por flanco 
	LPC_SC->EXTPOLAR|=(1<<3); //Interrupcion EXT3 activa por flanco ascendente
	//Borro los flags de inicio de EINT1-3
	LPC_SC->EXTINT|=(0xF);
	//Hacemos un Clear de las peticiones de interrupcion
	NVIC_ClearPendingIRQ(EINT0_IRQn);
	NVIC_ClearPendingIRQ(EINT1_IRQn);
	NVIC_ClearPendingIRQ(EINT2_IRQn);
	NVIC_ClearPendingIRQ(EINT3_IRQn);
	//Habilitación de las interrupciones
  NVIC_EnableIRQ(EINT0_IRQn);	// Habilitar interrupción EINT0
  NVIC_EnableIRQ(EINT1_IRQn);	// Habilitar interrupción EINT1
	NVIC_EnableIRQ(EINT2_IRQn);	// Habilitar interrupción EINT2
  NVIC_EnableIRQ(EINT3_IRQn);	// Habilitar interrupción EINT3
	//
}
void SysTick_Handler(void){
	static uint8_t display=0;
	tick++;
	LPC_GPIO0->FIOPIN =0xF0; //Apago los displays 
	LPC_GPIO1->FIOPIN &=~(1<<27); //Desactivar el punto
	if(flagext0==1){//Si activo la interrupion para la configuracion del generador
	seleccion++;//Esta variable se encarga de enceder el display correcto
		if(seleccion>4){ //Si estabamos en el display 4 antes de pulsar el boton me aseguro que en la siguiente pulsacion se eliga el display 1
		frecuencia=cg[3]+(cg[2]*10)+(cg[1]*100)+(cg[0]*1000); //Frecuencia generada almacenada en la variable frecuencia. De esta manera modifico los valores del MRO TIMER y por lo tanto, modifico la frecuncia
		ConfiguraTimer0(); //Reconfiguro el timer para la frecuencia escogida
		seleccion=0; //Pongo a 0 la seleccion del display, es decir, seleccion el primer display
		}
		flagext0=0;
	}
	if((seleccion!=0)){//Si estamos en el modo configuracion del generador
		visual[0]=segment[cg[0]];
		visual[1]=segment[cg[1]];
		visual[2]=segment[cg[2]];
		visual[3]=segment[cg[3]];
		if(seleccion==1){
			LPC_GPIO1->FIOPIN = (visual[3]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<7); //Enciendo el displays 
		}
		else if(seleccion==2){
			LPC_GPIO1->FIOPIN = (visual[2]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<6); //Enciendo el displays 
		}
		else if(seleccion==3){
			LPC_GPIO1->FIOPIN = (visual[1]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<5); //Enciendo el displays 
		}
		else if(seleccion==4){
			LPC_GPIO1->FIOPIN =(visual[0]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<4); //Enciendo el displays 
		}
	}else{
		if(display==0){
			LPC_GPIO1->FIOPIN = (visual[0]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<4); //Enciendo el displays 
			display=1;
		}
		else if(display==1){
			LPC_GPIO1->FIOPIN=(visual[1]<<20); //Activar para representar el valor que se representa en el display 
			if(aux2==1){
				LPC_GPIO1->FIOPIN |=(1<<27); //Activar el punto
			}
			LPC_GPIO0->FIOPIN &=~(1<<5); //Enciendo el displays 
			display=2;
		}
		else if(display==2){
			LPC_GPIO1->FIOPIN=(visual[2]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<6); //Enciendo el displays 
			display=3;
		}
		else if(display==3){
			LPC_GPIO1->FIOPIN=(visual[3]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<7); //Enciendo el displays 
			display=0;
		}
	}
}
void DAC_Handler(void){
	cont++;
	aux1=(LPC_GPIO0->FIOPIN >> 8 & 0x3); // se queda con la informacion de los bit 8 y 9 en la posicion 0 y 1, es decir el número que deseo
	switch(aux1){
			case 0://Selecciono el tipo de funcion, si es 00 función seno
				LPC_DAC->DACR=(FuncionSin[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 1://Selecciono el tipo de funcion, si es 01 función triángulo
				LPC_DAC->DACR=(FuncionTri[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 2://Selecciono el tipo de funcion, si es 10 función sierra
				LPC_DAC->DACR=(FuncionSierra[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 3: //Selecciono el tipo de funcion, si es 11 función 0V
				LPC_DAC->DACR=(Zero[i]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
		}
}
void TIMER0_IRQHandler(void){ //Se encarga del DAC, segun se modifica la frecuencia se modifica el Match
	if((LPC_TIM0->IR & 0x01) == 0x01){ //Si se produce el Match del Timer0
		LPC_TIM0->IR=1; //Borrar flag de interrupción MR0
		DAC_Handler();
	}
}
void EINT0_IRQHandler(void){ //Interrupción para la selección de los DISPLAYS
	LPC_SC->EXTINT |= (0<<1);   //Borrar el flag de la EINT1 --> EXTINT.0
	if(aux2==6){
		flagext0=1;
		/*seleccion++;//Esta variable se encarga de enceder el display correcto
		if(seleccion>4){ //Si estabamos en el display 4 antes de pulsar el boton me aseguro que en la siguiente pulsacion se eliga el display 1
		frecuencia=cg[3]+(cg[2]*10)+(cg[1]*100)+(cg[0]*1000); //Frecuencia generada almacenada en la variable frecuencia. De esta manera modifico los valores del MRO TIMER y por lo tanto, modifico la frecuncia
		ConfiguraTimer0(); //Reconfiguro el timer para la frecuencia escogida
		seleccion=0; //Pongo a 0 la seleccion del display, es decir, seleccion el primer display
		}*/
	}
}
void EINT1_IRQHandler(void){ //Interrupción incrementar el valor
	LPC_SC->EXTINT |= (0<<2);   //Borrar el flag de la EINT1 --> EXTINT.1
	//Si activo el puls_mas incremento en uno el valor que se muestra en el display
	flagext1=1;
	if(flagext1==1){ //Si se pulsa sumar
			if(seleccion==4){
				if(cg[0]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[0]++;
				}else{
					cg[0]=0;
				}
			}
			if(seleccion==3){
				if(cg[1]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[1]++;
				}else{
					cg[1]=0;
				}
			}
			if(seleccion==2){
				if(cg[2]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[2]++;
				}else{
					cg[2]=0;
				}
			}
			if(seleccion==1){
				if(cg[3]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[3]++;
				}else{
					cg[3]=0;
				}
			}
			flagext1=0;//Salgo del if para finalizar la ISR
		}
}
void EINT2_IRQHandler(void){ //Interrupción decrementar el valor
	LPC_SC->EXTINT |= (0<<3);   //Borrar el flag de la EINT2 --> EXTINT.2
	//Si activo el puls_menos decremento en uno el valor que se muestra en el display
	flagext2=1;
	if(flagext2==1){
			if(seleccion==4){
				if(cg[0]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[0]--;
				}else{
					cg[0]=9;
				}
			}
			if(seleccion==3){
				if(cg[1]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[1]--;
				}else{
					cg[1]=9;
				}
			}
			if(seleccion==2){
				if(cg[2]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[2]--;
				}else{
					cg[2]=9;
				}	
			}
			if(seleccion==1){
				if(cg[3]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[3]--;
				}else{
					cg[3]=9;
				}
			}
			flagext2=0;//Salgo del if para finalizar la ISR
		}
}
void EINT3_IRQHandler(void){ //Interrupción por flanco ascendente
	LPC_SC->EXTINT |= (0<<4);   //Borrar el flag de la EINT3 --> EXTINT.3
	numero_de_flancos++; //Incremento en 1 el valor cada vez que se produce una interrupció n por deteccion de flanco ascedente
}
void frecuency(void){
			LPC_GPIO2->FIOPIN &=~ led_frecuencimetro;   // Activar el LED FRECUENTIMETRO	
			if(tick>=100){ //Despues de que se produzcan 100 ticks, obtengo la frecuencia capturada por el microprocesador e inicio de nuevo la cuenta del numero de flancos
				FrecuenciaKHz=(numero_de_flancos*2)/10;
				numero_de_flancos=0;
				tick=0;
			}
			if(FrecuenciaKHz<1){ //Si la frecuencia de la señal de entrada es menor de 10Hz
				for(i=0;i<4;i++){//Representar 0000 en el Display 
					visual[i]=segment[0]; //Para visualizar 00.00
				}
			}
			if((FrecuenciaKHz>=1)&&(FrecuenciaKHz<=9999)){ //Si la frecuencia esta comprendida entre el minimo visible (10Hz) y el maximo visible (99.99KHz)
				cf[0]=FrecuenciaKHz/1000; //Unidad de millar
				cf[1]=(FrecuenciaKHz-cf[0]*1000)/100; //Centenas
				cf[2]=(FrecuenciaKHz-cf[0]*1000-cf[1]*100)/10; //Decenas
				cf[3]=(FrecuenciaKHz-cf[0]*1000-cf[1]*100-cf[2]*10);//Unidades
				visual[0]=segment[cf[0]];
				visual[1]=segment[cf[1]];
				visual[2]=segment[cf[2]];
				visual[3]=segment[cf[3]];
			}
			if(FrecuenciaKHz>9999){ //Si la frecuencia es mayor de 99.99KHz
				for(i=0;i<4;i++){ //Representar HH.HH en el Display
					cf[i]=HHHH[i];
				}
			}
			LPC_GPIO2->FIOPIN |= led_frecuencimetro;   //Desactivo el pin LED FRECUENTIMETRO
}
void generador(void){
		LPC_GPIO2->FIOPIN &=~ led_generador; //Enciendo el LED del generador
		visual[0]=segment[cg[0]];
		visual[1]=segment[cg[1]];
		visual[2]=segment[cg[2]];
		visual[3]=segment[cg[3]];
		if(aux1==0){ //Si esta activa la funcion seno
			LPC_GPIO2->FIOPIN&=~led_seno;//Enciendo el Led que indica que el tipo de función es la sinosoidal
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra;//Me aseguro que apago los Leds de los otros tipos de funcion
		}
		if(aux1==1){ //Si esta activa la funcion triangular
			LPC_GPIO2->FIOPIN&=~led_triangulo;//Enciendo el Led que indica que el tipo de función es la triangular
			LPC_GPIO2->FIOPIN|=led_seno|led_sierra;//Me aseguro que apago los Leds de los otros tipos de funcion
		}
		if(aux1==2){ //Si esta activa la funcion Sierra
			LPC_GPIO2->FIOPIN&=~led_sierra;//Enciendo el Led que indica que el tipo de función es la Diente Sierra
			LPC_GPIO2->FIOPIN|=led_triangulo|led_seno;//Me aseguro que apago los Leds de los otros tipos de funcion
		}
		LPC_GPIO2->FIOPIN |= led_generador;   // Desactivo el pin LED GENERADOR
}
void visualizar(void){
	aux2=(LPC_GPIO2->FIOPIN >>0 & 0x7);
	switch(aux2){
		case 0: //OFF frecuencimetro, OFF generador. Visualizar HOLA
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra|led_seno;//Me aseguro que apago los Leds de los tipos de funcion
			for(i=0;i<4;i++){ //Realizo un ciclo para que se active el display que toque y enseñe la letra que toca en orden
				visual[i]=HOLA[i];
			}
			break;
		case 1://ON frecuencimetro, OFF generador. Visualizar Frecuencimetro. Modo Frecuencia
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra|led_seno;//Me aseguro que apago los Leds de los tipos de funcion
			frecuency();
			break;
		case 2: //Visualizar Frecuencimetro, ON generador, OFF frecuencimetro. Visualizar Gen_
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra|led_seno;//Me aseguro que apago los Leds de los tipos de funcion
			for(i=0;i<4;i++){ //Realizo un ciclo para que se active el display que toque y enseñe la letra que toca en orden
				visual[i]=Gen[i];
			}
			break;
		case 5://Visualizar Generador, OFF generador, ON Frecuencimetro. Visualizar FrEC
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra|led_seno;//Me aseguro que apago los Leds de los tipos de funcion
			for(i=0;i<4;i++){ //Realizo un ciclo para que se active el display que toque y enseñe la letra que toca en orden
				visual[i]=frec[i];
			}
			break;
		case 6://OFF Frecuencimetro, ON Generador. Modo Generador
			LPC_GPIO2->FIOPIN|=led_triangulo|led_sierra|led_seno;//Me aseguro que apago los Leds de los tipos de funcion
			generador();
			break;
	}
}
int main(){
	configGPIO(); //Llamada a la función configuración GPIO
	ConfigDAC(); //Llamada a la configuracion del DAC
	Configura_SysTick(); //Llamada a la funcion configuracion del SysTick
	ConfiguraTimer0(); //Llamada a la función configuración Timer0
	ConfiguracionEXT(); //Llamada a la funcion configuracion y habilitacion de las interrupciones
	Funciones(); //Llamada a la funcion que contiene el desarrollo matematico para poder representar las Ondas
	while(1){ //Bucle principal indefinido
		visualizar(); //Llamada a la función del programa principal
	}
}
