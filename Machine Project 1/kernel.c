extern void keyboard_handler(void);
extern void timer_handler(void);

struct IDT_entry{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};
struct IDT_entry IDT[256];

struct marquee{
	int posX;
	int posY;
	char text[80];
};

char* header = "TheOS: ";

char* vidPtr = (char*)0xb8000;
unsigned int vidCtr = 0;

int newLine = 0;
char keyPress;
unsigned int commandIndex = 0;
char* command;

unsigned int cursorX = 0;
unsigned int cursorY = 0;
unsigned short cursorLoc = 0;

int timerTicks = 0;
struct marquee marArray[24];
int marCount = 0;
char* marText;

unsigned char keyboard[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',						/* Backspace */
	'\t',											/* Tab */
	'q', 'w', 'e', 'r',								/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	0,												/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   0,									/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',				/* 49 */
	'm', ',', '.', '/',   0,						/* Right shift */
	'*',
	0,												/* Alt */
	' ',											/* Space bar */
    0,												/* Caps lock */
    0,												/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,												/* < ... F10 */
    0,												/* 69 - Num lock*/
    0,												/* Scroll Lock */
    0,												/* Home key */
    0,												/* Up Arrow */
    0,												/* Page Up */
	'-',
    0,												/* Left Arrow */
    0,
    0,												/* Right Arrow */
	'+',
    0,												/* 79 - End key*/
    0,												/* Down Arrow */
    0,												/* Page Down */
    0,												/* Insert key*/
    0,												/* Delete key */
    0,   0,   0,
    0,												/* F11 key */
    0,												/* F12 key */
    0,												/* All other keys are undefined */
};

void idt_init(void){
	unsigned long keyboard_address;
	unsigned long timer_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];
	
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = 0x08;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = 0x8e;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;
	
	timer_address = (unsigned long)timer_handler;
	IDT[0x20].offset_lowerbits = timer_address & 0xffff;
	IDT[0x20].selector = 0x08;
	IDT[0x20].zero = 0;
	IDT[0x20].type_attr = 0x8e;
	IDT[0x20].offset_higherbits = (timer_address & 0xffff0000) >> 16;
	
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);
	
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * 256) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;
	load_idt(idt_ptr);
}

void timer_init(void){
	write_port(0x21, 0xFC);
}

void kb_init(void){
	write_port(0x21, 0xFD);
}

void outb(unsigned short port, unsigned char value){
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

unsigned char inb(unsigned short port){
	unsigned char ret;
	asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

unsigned short inw(unsigned short port){
	unsigned short ret;
	asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

int strlen(char* str){
	int count = 0;
	
	while(str[count] != '\0')
		count++;
	
	return count;
}

int strcmp(char *str1, char *str2){
    int len1 = strlen(str1);
    int len2 = strlen(str2);
    int i;
	
    for(i = 0; i < len1 && i < len2; i++){
    	if(str1[i] != str2[i])
    		return str1[i] - str2[i];
    }
	
    return len1 - len2;
}

void moveCursor(){
	if(cursorX >= 160){
		cursorX = strlen(header) * 2;
		cursorY++;
	}
	
	if(cursorY == 23 && newLine == 0){
    	cursorLoc = (cursorY * 80) + (cursorX / 2);
	} else if(cursorY == 23 && newLine == 1){
    	cursorLoc = ((cursorY + 1) * 80) + (cursorX / 2);
    	newLine = 0;
    } else
   		cursorLoc = (cursorY * 80) + (cursorX / 2);
	
   	outb(0x3D4, 14);
   	outb(0x3D5, cursorLoc >> 8);
   	outb(0x3D4, 15);
   	outb(0x3D5, cursorLoc);
}

void scroll(){
	int i;	
	
    if(cursorY >= 24){
    	for(i = 0; i < 25 * 160; i += 2)
    		vidPtr[i] = vidPtr[i + 160];
		
    	cursorY = 23;
		
    	for(i = 0; i < marCount; i++)
    		marArray[i].posY--;
	}
	
	moveCursor();
}

void printStr(char* string){
	int i = 0;	
	
	if(string[0] == '\n'){
		vidCtr = (cursorY + 1) * 160;
		cursorX = 0;
		cursorY++;
		newLine = 1;
	} else{
		while(string[i] != '\0'){
			if(string[i] == '\n'){
				i++;
				vidCtr = (cursorY + 1) * 160;
				cursorX = 0;
				cursorY++;
			}	
			
			vidPtr[vidCtr] = string[i];
			vidPtr[vidCtr+1] = 0x07;
			i++;
			vidCtr = vidCtr + 2;
		}
		cursorX = vidCtr - (cursorY * 160);
	}
	
	scroll();
}

void printChar(char c){
	vidCtr = (cursorX) + (cursorY * 160);
	
	if(c == '\n'){
		vidCtr = (cursorY + 1) * 160;
		
		if(cursorY == 24)
			cursorY += 2;
		else cursorY++;
		
		printStr(header);
		command[commandIndex] = '\0';
	}
	
	else if(c == '\b'){
		if(cursorX > (strlen(header) * 2)){
			command[commandIndex] = '\0';
			vidPtr[vidCtr-2] = '\0';
			vidPtr[vidCtr-1] = 0x07;
	    	cursorX -= 2;
			vidCtr -= 2;
			commandIndex--;
		}
	}
	
	else{
		command[commandIndex] = c;
		vidPtr[vidCtr] = c;
		vidPtr[vidCtr+1] = 0x07;
	    cursorX += 2;
		vidCtr += 2;
		commandIndex++;
	}
	
	scroll();
}

void printCharAtXY(char c, int x, int y){
	int loc;
	
	loc = (x * 2) + (y * 160);
	vidPtr[loc] = c;
	vidPtr[loc+1] = 0x07;
}

void clearMarquee(){
	int i, j;
	
	for(i = 0; i < marCount; i++){
		for(j = 0; j < strlen(marArray[i].text); j++)
			marArray[i].text[j] = ' ';
		
		marArray[i].posX = 0;
		marArray[i].posY = 0;
	}
	
	marCount = 0;
}

void clearScreen(){
	int i;
	
	while(i < 4000){
		vidPtr[i] = ' ';
		vidPtr[i+1] = 0x07;
		i = i + 2;
	}
	
	vidCtr = 0;
	cursorX = 0;
	cursorY = 0;
	
	clearMarquee();
	idt_init();
	kb_init();
	printStr(header);
}

void clearLine(int y){
	int i, currPos;
	currPos = (160 * y);
	
	for(i = currPos; i < currPos + 160; i++){
		vidPtr[i] = ' ';
		vidPtr[i+1] = 0x07;
		i++;
	}
}

void saveText(struct marquee *marquee, char* string){
	int i;
	
	for(i = 0; i < strlen(string); i++)
		marquee->text[i] = string[i];
}

void printMarquee(struct marquee *marquee){
	int i, currX;
	clearLine(marquee->posY);
	currX = marquee->posX;
	i = 0;
	
	while(marquee->text[i] != '\0'){
		if(currX >= 80)
			currX -= 80;
		
		printCharAtXY(marquee->text[i], currX, marquee->posY);
		currX++;
		i++;
	}
	
	if(marquee->posX >= 80)
		marquee->posX = 0;
	
	marquee->posX += 1; 
}

void addMarquee(){
	timer_init();
	marArray[marCount].posX = 0;
	marArray[marCount].posY = cursorY;
	saveText(&marArray[marCount], marText);
	marCount++;
	printChar('\n');
}

void timer(){
	int i;
	outb(0x20, 0x20);
    timerTicks++;
	
    if((timerTicks % 2) == 0){
    	if(marCount > 0){
    		for(i = 0; i < marCount; i++)
       			printMarquee(&marArray[i]);
       	}
       	else printMarquee(&marArray[0]);
    }
}

void checkCommand(){
	char talk[80];
	char check[80];
	int i = 0;
	
	/*checks for say*/
	for(i = 0; i < 4; i++)
		talk[i] = command[i];
	talk[i] = '\0';
	
	/*checks for marquee*/
	for(i = 0; i < 8; i++)
		check[i] = command[i];
	check[i] = '\0';
	
	if(strcmp("cls", command) == 0){
		clearScreen();
	}
	
	else if(strcmp("say ", talk) == 0){
		printStr(command + 4);
		printChar('\n');
	}
	
	if(strcmp("marquee ", check) == 0){
		for(i = 0; i < commandIndex - 8; i++)
			marText[i] = command[i+8];
		marText[i] = '\0';
		addMarquee();
	}
	commandIndex = 0;
}

void keyCall(){
	unsigned char status;
	write_port(0x20, 0x20);
	status = read_port(0x64);
	
	if (status & 0x01){
		keyPress = read_port(0x60);
		
		if(keyPress < 0)
			return;
		
		printChar(keyboard[keyPress]);
		
		if(keyPress == 28)
			checkCommand();
	}
}

void kmain(void){
	clearScreen();
	scroll();
	idt_init();
	kb_init();
	while(1);
}
