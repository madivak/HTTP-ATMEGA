
/*
 * GPS-UART-BUF.c
 *
 * Created: 08/05/2018 5:37:46 PM
 * Author : Madiva
 */ 
#define F_CPU 1000000UL
#include <string.h>
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <ctype.h> //for identifying digits and alphabets
#include "USART.h"
#include "sdcard.h"

#define FOSC 2000000UL // Clock Speed
#define BAUD 9600
#define MYUBRR (((FOSC / (BAUD * 16UL))) - 1)

char byteGPS=-1;
char linea[100] = "";
char satellites[2] = "";
//char IP[21] = "";
//char MemIP[23] ="";
//char FinalIP[23] = "";
//char ExlonIP[] ="XXX.XXX.XXX.XXX\",\"XXXX";
char comandoRMC[7] = "$GPRMC";
char comandoGGA[7] = "$GPGGA";
char *RMC, *GGA, w;
char status[4], phone[13], *Digits;
int GPS_position_count=0;
int datacount=0, datacount1=0;
int x, y, a, i, b;
int cont=0, bien=0, bien1=0, conta=0;
char fix; int e;

char input;
char buff[20];
char company[]	= "+254XXXXXXXXX";// //Moha's No#
char company2[]	= "+254700000000";// //Fatah's no#
char owner[]	= "+254XXXXXXXXX"; //Kevin's no#

//static FILE uart0_output = FDEV_SETUP_STREAM(USART0_Transmit, NULL, _FDEV_SETUP_WRITE);
//static FILE uart1_output = FDEV_SETUP_STREAM(USART0_Transmit, NULL, _FDEV_SETUP_WRITE);
static FILE uart1_output = FDEV_SETUP_STREAM(USART1_Transmit, NULL, _FDEV_SETUP_WRITE);
static FILE uart1_input = FDEV_SETUP_STREAM(NULL, USART1_Receive, _FDEV_SETUP_READ);
static FILE uart0_input = FDEV_SETUP_STREAM(NULL, USART0_Receive, _FDEV_SETUP_READ);

void HTTPTransmit1 ();
void HTTPTransmit2 ();
void grabGPS();
unsigned char CheckSMS();
void checknewSMS();
unsigned char sender();
unsigned char CompareNumber();
unsigned char initialstatus();
void PrintSender();
void CreateDraft(char m);
int checkOKstatus(int p);

#define CAR_ON	PORTA |= (1<<PORTA3)
#define CAR_OFF	PORTA &= ~(1<<PORTA3)
#define ACC_ON	PORTB |= (1<<PORTB0)
#define ACC_OFF	PORTB &= ~(1<<PORTB0)

#define CHOSEN_SECTOR 0
#define BUFFER_SIZE 24



int main( void )
{
 	CAR_OFF;
	ACC_OFF;
	/**************************SD-CODE-TESTING************************************/
	/* LED for result feedback PA3 (active High) */
	DDRA |= (1 << 3);		//set 3bit of Data Direction Register Aas output 
	DDRA &= ~(1<<DDA0);     //set bit 0 of DDRA to input
	PORTA |= (1 << 3);		//set PORTA bit 3 as HIGH
	DDRA &= ~(1 << 5);
	_delay_us(1000);

	USART1_Init(MYUBRR);
	USART0_Init(MYUBRR);
	DDRA |= (1<<DDA3); //set PORTB1 as output
	DDRB |= (1<<DDB0); //set PORTB0 AS acc output
	
 	stdin = &uart0_input;
 	stdout = &uart1_output; //changed to TX1 for GSM communication. TX0 on Atmega SMD isnt working
	 
// 	PCMSK0 |= (1<<PCINT0);
// 	PCICR |= (1<<PCIE0);
// 	sei(); //enable blobal interrupts
	
	_delay_ms(13000);
			printf("AT\r\n");
			_delay_ms(2000);
			printf("AT+CMGF=1\r\n");
			_delay_ms(2000);
	
	initialstatus();
	int S=0;
	
	while(1) 
	{
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		HTTPTransmit1 ();
		fdev_close();
		stdout = &uart1_output;
		stdin = &uart1_input;
	
		grabGPS();
		_delay_ms(500);
			
		fdev_close();
		stdout = &uart1_output; 
		stdin = &uart0_input;
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		HTTPTransmit2 ();
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		checknewSMS();
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		initialstatus();
		S++;
		if (S==100)
		{ S=0; /*printf("AT+CFUN=0\r\n"); _delay_ms(2000);*/ printf("AT+CFUN=1\r\n");  _delay_ms(10000); } 
		else { }
		
	}
	return 0;
}

// ISR(PCINT0_vect)
// {
// 	while (!(PINA & (1<<PINA0))) //While the phone is ringing
// 	{	_delay_ms(5000); }
// 
// 	PINB |= (1<<PINB1);
// 	printf("AT\r\n");
// 	_delay_ms(1000);
// }

int checkOKstatus(int p)
{
	b=0;
	w = getchar();
	while (w!=0x0A) // skip preceding '/n'
	{	w = getchar();	}//
		
	if (p==1) //if we are only checking for Ok status
	{
		w = getchar();
		if (w==0x04F) // if next character is 'O'
		{	w = getchar();
			if (w==0x04B) {	w = getchar();	b=1;} // if w = K
			else{b=2;}
		}//
		else{b=2;}
	}//
	else if(p==2)//if we are checking for "SHUT OK"
	{
		for (int i=1; i<6;i++) //skip through 1st 5 char of "SHUT OK"
		{ w = getchar();}
			
		w = getchar();
		if (w==0x04F) // if next character after 'SHUT'is 'O'
		{	w = getchar();
			if (w!=0x04B) {	w = getchar();	b=1;} // if w = K
			else{b=2;}
		}
		else{b=2;}
	}
	else if(p==3)//if we are checking for new sms
	{
			w = getchar();
			while (w !=  0x2B) //w is not +
			{ w = getchar();}
			int q=0;
			while (q < 7)
			{	w = getchar();
				if (w==0x2C) { q++;} //if w is "," in the string +CPMS: "MT",1,75,"SM",1,25,"ME",0,50
				else{}
			}
			w = getchar();
			if (w != 0x30)	//if after 7th "," in ("MT",1,75,"SM",1,25,"ME",0,50) next character is not 0
			{CheckSMS();}
			else {}
	}
	else if(p==4) //Run through until u get the '+' in "+HTTPACTION"
	{
		int V=0;
		w = getchar();
		if (byteGPS == -1)
			{/*empty port*/_delay_ms(2000);}
		else
		{
// 			while(V==0) ///////////////////
// 			{ 	if (w==':')		{V=1;}
// 			else if(w=='E')	{V=2;}
// 			w = getchar();
			while (V<2)
			{ if(w==0x0A) {V++;} w = getchar();}
		}

	}
	
	else{}
	_delay_ms(500);
 return b;
}

unsigned char CheckSMS()
{
	int z = 0; //char w;
	y=0;
	a=0;
	printf("AT\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
  	checkOKstatus(1);
	 _delay_ms(500);
	printf("AT+CMGF=1\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
  	checkOKstatus(1);
	 _delay_ms(500);
	 while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	printf("AT+CMGL=\"REC UNREAD\"\r\n");
	while (a < 2) //skip the <LF>
	{
		w = getchar();
		if (w==0x0A) { a++; }
		else {}
	}
	w = getchar();
	
	if (w==0x02B) // if w = +
	{
		sender();
		w = getchar();
		while (w !=  0x0A) //w is not <LF>
		{ w = getchar();}
		
		w = getchar();
		//if ((w == 0x030) || (w == 0x031))//w is '0' or '1'
		if (w==0x030 || w==0x031)//w is '0' or '1'
		{
			CompareNumber();
			z = status[1] + status[2] + status[3]; //sum values of the 3 buffer values
			if (z < 3) //A scenario of receiving text from an authorized no# with '1' or '0'
			{	status[0] = w;
				if ( w==0x030)		{CAR_OFF; ACC_OFF;}	//If the text received is a '0'
				else if ( w==0x031)	{CAR_ON; ACC_ON;}	 //If the text received is a '1'
				else{}
				CreateDraft(w);
			}
			else //A scenario of receiving text from Unauthorized no#
			{	status[0] = 2; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);} //clearing all SMS in storage AREA except Draft and UNREAD SMS
		}
		else if (w==0x24) //If a $ is received
		{
//			IP_Change_Command();
			status[0] = 6; status[1] = status[2] = status[3] = 0; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);
		}
		else
		{	status[0] = 6; status[1] = status[2] = status[3] = 0; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);} //clearing all SMS in storage AREA except Draft and UNREAD SMS
	}
	else if(w==0x04F) // if w = 'O'
	{
		w = getchar();
		if (w==0x04B) // if w = 'K'  If there is no new message
		{	status[0] = 3; status[1] = status[2] = status[3] = 0; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);}
		else
		{	status[0] = 4; status[1] = status[2] = status[3] = 0; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);}
	}
	else {	status[0] = 5; status[1] = status[2] = status[3] = 0; printf("AT+CMGD=1,2\r\n"); while (!(PINA & (1<<PINA0))) { }; _delay_ms(2000);} 

	return *status;
}


//READ DRAFT WHEN SIM IS CREDITED TO AVOID ERROR DURING DATA TRANSMISSION .......TO BE DONE


void checknewSMS()
{
		printf("AT\r\n");
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		checkOKstatus(1);
		printf("AT+CMGF=1\r\n");
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		checkOKstatus(1);
		printf("AT+CPMS?\r\n");
		while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
		checkOKstatus(3);
		_delay_ms(1000);
}

void CreateDraft(char m)
{
	printf("AT+CMGD=1,4\r\n"); //clearing all SMS in storage AREA
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1);
	printf("AT+CMGW=\"");
	PrintSender();
	printf("\",145,\"STO UNSENT\"\r\n%c",m);
	_delay_ms(2000);
	putchar(0x1A); //putting AT-MSG termination CTRL+Z in USART0
	checkOKstatus(1);
}

unsigned char sender()
{
	int n;
	w = getchar();
	while (w != 0x02B) // while w is not +
	{ w = getchar(); }

	for (n=0; n<13; n++) //capture 13 digit phone number
	{	phone[n] = w; w = getchar(); }
	
	return *phone;
}

void PrintSender()
{
	int n;
	for (n=1; n<13; n++) //for 13 digit phone number //something fishy :D if u begin from n=0 we'll have ++ before the no#
	{	printf("%c", phone[n]);}
}

unsigned char CompareNumber()
{
	int j;
	status[1] = status[2] = status[3] =0;
	
	for (j=0; j<13; j++)
	{
		if (phone[j]!=company[j])
		{ status[1] = 1;}
		else{}
		if (phone[j]!=owner[j])
		{ status[2] = 1;}
		else{}
		if (phone[j]!=company2[j])
		{ status[3] = 1;}
		else{}
	}
	return *status;
}

void HTTPTransmit1 () //char *GPS0, char *GPS1, char *number)
{
	printf("AT\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
  	checkOKstatus(1); _delay_ms(500);
	printf("AT+CGATT=1\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);;
	printf("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);
	printf("AT+SAPBR=3,1,\"APN\",\"safaricom\"\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);
	printf("AT+SAPBR=1,1\r\n");
//	checkOKstatus(1);
	_delay_ms(8000);
	printf("AT+HTTPINIT\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);
	printf("AT+HTTPPARA=\"CID\",1\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);
}

void HTTPTransmit2 ()
{
	printf("AT+HTTPSSL=1\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(1000);
	printf("AT+HTTPACTION=1\r\n"); //Ends with "+HTTPACTION" after 4 X "/n"
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(8000);
//	checkOKstatus(4);
// 	printf("AT+HTTPREAD\r\n"); ////////////////////////Starts With +HTTPREAD
// 	checkOKstatus(4);
// 	_delay_ms(1000);
	printf("AT+HTTPTERM\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1); _delay_ms(500);
	printf("AT+SAPBR=0,1\r\n");
//	checkOKstatus(1);
	_delay_ms(8000);	
}

unsigned char initialstatus()
{
	//char w;
	y=0;
	a=0;
	printf("AT\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
  	//checkOKstatus(1);
  	_delay_ms(2000);
	putchar(0x1A); //putting AT-MSG termination CTRL+Z in USART just in case it hangs at message or TCP sending
	_delay_ms(2000);
	
	printf("AT+CMGF=1\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	checkOKstatus(1);
	printf("AT+CPMS=\"MT\",\"SM\",\"ME\"\r\n");
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	_delay_ms(2000);
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing
	printf("AT+CPMS?\r\n");
	w = getchar();
	while (w !=  0x2B) //w is not +
		{ w = getchar();}
	int q=0;
	while (q<7)
	{	w = getchar();
		if (w==0x2C) { q++;} //if w is "," in the string +CPMS: "MT",1,75,"SM",1,25,"ME",0,50
		else{}
	}
	w = getchar();
	if (w != 0x30)	{CheckSMS();}
	else {}	
		_delay_ms(500);
	//_delay_ms(2000);
	while (!(PINA & (1<<PINA0))) { } //While the phone is ringing	
	printf("AT+CMGR=1\r\n");
	while (a < 2) //skip the <LF>
	{
		w = getchar();
		if (w==0x0A)
		{ a++; }
		else
		{}
	}
	w = getchar();
	
	if (w==0x02B) // if w = +
	{
		sender();
		w = getchar();
		while (w !=  0x0A) //w is not <LF>
		{ w = getchar();}
		
		w = getchar();
		if (w==0x030 || w==0x031)//w is '0' or '1'
		{	status[0] = w; 
			_delay_ms(200); 
			if (w==0x030)		{CAR_OFF;}
			else if (w==0x031)	{CAR_ON;}
			else{}
		}			
		else{ status[0] = 6; }
	}
	else{}

	return *status;
}


void grabGPS()
{
  y=0; x=0;
  while(x==0 || y==0)
	{
	byteGPS=getchar();
	// Read a byte of the serial port
	if (byteGPS == -1)
	{  /* See if the port is empty yet*/ }
	else
	{
		// note: there is a potential buffer overflow here!
		linea[conta]=byteGPS;        // If there is serial port data, it is put in the buffer
		conta++;
		datacount++;
		datacount1++;
		
		//Serial.print(byteGPS);    //If you delete '//', you will get the all GPS information
		
		if (byteGPS==13)
		{
			// If the received byte is = to 13, end of transmission
			// note: the actual end of transmission is <CR><LF> (i.e. 0x13 0x10)
			cont=0;
			bien=0;
			bien1=0;
			// The following for loop starts at 1, because this code is clowny and the first byte is the <LF> (0x10) from the previous transmission.
			for (int i=1;i<7;i++)     // Verifies if the received command starts with $GPGGA
			{
				if (linea[i]==comandoGGA[i-1])
				{bien++;}
			}
			for (int i=1;i<7;i++)     // Verifies if the received command starts with $GPRMC
			{
				if (linea[i]==comandoRMC[i-1])
				{bien1++;}
			}
			
			//-----------------------------------------------------------------------
			if(bien==6) // If initial characters match "+GPGGA" string, process the data
			{
				//linea[0]='\n';
				int p=0;
				int u=0;
				e=0;
				satellites[0]='\0';
				satellites[1]='\0';

			for (int i=1;i<datacount;i++)
			{
				if (linea[i]==0x2C)
				{e++; if (e==6) {p=i;} if (e==7) {u=i;}}	
			}			
				fix=linea[p+1];
				satellites[0]=linea[u+1];
				satellites[1]=linea[u+2];
							
				GGA = (char*) malloc(datacount+1);			//allocating memory to the GGA string, where datacount is length of GPS GGA string
				strcpy(GGA,linea);							//using function strcpy to copy the char array(linea) to GGA string which we have allocated memory above
				y=1;										//one half of the conditions to exit while(x==0 || y==0) function
			}
			if(bien1==6) // If initial characters match "+GPRMC" string, process the data
			{
				//linea[0]='\n';
				linea[0]=0x2D;
				linea[datacount1]=0x00;
				linea[datacount1-1]=0x00;
				RMC = (char*) malloc(datacount1+1);
				strcpy(RMC,linea);
				x=1;
			}

			if (x==1 && y==1)
			{
				phone[13]=0x00;
				Digits = (char*) malloc(13);//allocate memory for the phone number that text the tracker 
				strcpy(Digits,phone);		//save the number that texted to a char string
				
				_delay_ms(500);
//				printf("AT+HTTPPARA=\"URL\",\"www.SITE.com/api/events?ID=201800002&String=%c:%s:%c:%c%c:%s\"\r\n",status[0],Digits,fix, satellites[0], satellites[1], RMC);
				printf("AT+HTTPPARA=\"URL\",\"http://SITE.com/t/exlon/post?ID=201800001&String=%c:%s:%c:%c%c:%s\"\r\n",status[0],Digits,fix, satellites[0], satellites[1], RMC);
//				printf("AT+HTTPPARA=\"URL\",\"http://SITE.com/t/madiva/post?ID=201800001&String=%c:%s:%c:%c%c:%s\"\r\n",status[0],Digits,fix, satellites[0], satellites[1], RMC);
//				sample_GPS_data(GGA,RMC,Digits);
				_delay_ms(2000);
				free(GGA); //The memory location pointed by GGA is freed. Otherwise it will cause error
				free(RMC); //The memory location pointed by RMC is freed. Otherwise it will cause error
				free(Digits);
				_delay_ms(500);
			}
			else{}
			//-----------------------------------------------------------------------
			conta=0;
			datacount=0;
			datacount1=0;
			// Reset the buffer
			for (int i=0;i<100;i++)
			{    //
				linea[i]='\0';
			}
		} //byteGPS==13
	}  //else byteGPS is not null
} //Serial1.available
//return 0;
} //testGPS

