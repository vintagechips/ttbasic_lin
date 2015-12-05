/*
	TOYOSHIKI TinyBASIC V1.0
	Linux edition
	(C)2015 Tetsuya Suzuki
*/

// Compiler requires description
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

// TOYOSHIKI TinyBASIC symbols
// TO-DO Rewrite defined values to fit your machine as needed
#define SIZE_LINE 80 //Command line buffer length + NULL
#define SIZE_IBUF 80 //i-code conversion buffer size
#define SIZE_LIST 1024 //List buffer size
#define SIZE_ARRY 64 //Array area size
#define SIZE_GSTK 6 //GOSUB stack size(2/nest)
#define SIZE_LSTK 15 //FOR stack size(5/nest)

// Depending on device functions
// TO-DO Rewrite these functions to fit your machine
#define STR_EDITION "LINUX"

// Terminal control
#define c_putch(c) putchar(c)

char c_getch(){
	struct termios b;
	struct termios a;
	char c;

	tcgetattr(STDIN_FILENO, &b);
	a = b;
	a.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &a);
	c = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &b);

	return c;
}

char c_kbhit(void)
{
	char c;
	int f;

	f = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);

	c = c_getch();

	fcntl(STDIN_FILENO, F_SETFL, f);

	if (c != EOF) {
		ungetc(c, stdin);
		return 1;
	}

	return 0;
}

#define KEY_ENTER 10
void newline(void){
	c_putch(KEY_ENTER); //LF
}

// Return random number
short getrnd(short value){
        return(rand() % value) + 1;
}

// Prototypes (necessity minimum)
short iexp(void);

// Keyword table
const char* kwtbl[] = {
	"GOTO", "GOSUB", "RETURN",
	"FOR", "TO", "STEP", "NEXT",
	"IF", "REM", "STOP",
	"INPUT", "PRINT", "LET",
	",", ";",
	"-", "+", "*", "/", "(", ")",
	">=", "#", ">", "=", "<=", "<",
	 "@", "RND", "ABS", "SIZE",
	"LIST", "RUN", "NEW", "SYSTEM"
};

// i-code(Intermediate code) assignment
enum{
	I_GOTO, I_GOSUB, I_RETURN,
	I_FOR, I_TO, I_STEP, I_NEXT,
	I_IF, I_REM, I_STOP,
	I_INPUT, I_PRINT, I_LET,
	I_COMMA, I_SEMI,
	I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE,
	I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
	I_ARRAY, I_RND, I_ABS, I_SIZE,
	I_LIST, I_RUN, I_NEW, I_SYSTEM,
	I_NUM, I_VAR, I_STR,
	I_EOL
};

// Keyword count
#define SIZE_KWTBL (sizeof(kwtbl) / sizeof(const char*))

// List formatting condition
// no space after
const unsigned char i_nsa[] = {
	I_RETURN, I_STOP, I_COMMA,
	I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE,
	I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
	I_ARRAY, I_RND, I_ABS, I_SIZE
};

// no space before (after numeric or variable only)
const unsigned char i_nsb[] = {
	I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE,
	I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
	I_COMMA, I_SEMI, I_EOL
};

// exception search function
char sstyle(unsigned char code, const unsigned char *table, unsigned char count) {
	while(count--)
		if (code == table[count])
			return 1;
	return 0;
}

// exception search macro
#define nospacea(c) sstyle(c, i_nsa, sizeof(i_nsa))
#define nospaceb(c) sstyle(c, i_nsb, sizeof(i_nsb))

// Error messages
unsigned char err;// Error message index
const char* errmsg[] ={
	"OK",
	"Devision by zero",
	"Overflow",
	"Subscript out of range",
	"Icode buffer full",
	"List full",
	"GOSUB too many nested",
	"RETURN stack underflow",
	"FOR too many nested",
	"NEXT without FOR",
	"NEXT without counter",
	"NEXT mismatch FOR",
	"FOR without variable",
	"FOR without TO",
	"LET without variable",
	"IF without condition",
	"Undefined line number",
	"\'(\' or \')\' expected",
	"\'=\' expected",
	"Illegal command",
	"Syntax error",
	"Internal error",
	"Abort by [ESC]"
};

// Error code assignment
enum{
	ERR_OK,
	ERR_DIVBY0,
	ERR_VOF,
	ERR_SOR,
	ERR_IBUFOF, ERR_LBUFOF,
	ERR_GSTKOF, ERR_GSTKUF,
	ERR_LSTKOF, ERR_LSTKUF,
	ERR_NEXTWOV, ERR_NEXTUM, ERR_FORWOV, ERR_FORWOTO,
	ERR_LETWOV, ERR_IFWOC,
	ERR_ULN,
	ERR_PAREN, ERR_VWOEQ,
	ERR_COM,
	ERR_SYNTAX,
	ERR_SYS,
	ERR_ESC
};

// RAM mapping
char lbuf[SIZE_LINE]; //Command line buffer
unsigned char ibuf[SIZE_IBUF]; //i-code conversion buffer
short var[26]; //Variable area
short arr[SIZE_ARRY]; //Array area
unsigned char listbuf[SIZE_LIST]; //List area
unsigned char* clp; //Pointer current line
unsigned char* cip; //Pointer current Intermediate code
unsigned char* gstk[SIZE_GSTK]; //GOSUB stack
unsigned char gstki; //GOSUB stack index
unsigned char* lstk[SIZE_LSTK]; //FOR stack
unsigned char lstki; //FOR stack index

// Standard C libraly (about) same functions
char c_toupper(char c) {return(c <= 'z' && c >= 'a' ? c - 32 : c);}
char c_isprint(char c) {return(c >= 32  && c <= 126);}
char c_isspace(char c) {return(c == ' ' || (c <= 13 && c >= 9));}
char c_isdigit(char c) {return(c <= '9' && c >= '0');}
char c_isalpha(char c) {return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A'));}
void c_puts(const char *s) {while(*s) c_putch(*s++);}
void c_gets(){
	char c;
	unsigned char len;

	len = 0;
	while((c = c_getch()) != KEY_ENTER){
		if( c == 9) c = ' '; // TAB exchange Space
		if(((c == 8) || (c == 127)) && (len > 0)){ // Backspace manipulation
			len--;
			c_putch(8); c_putch(' '); c_putch(8);
		} else
		if(c_isprint(c) && (len < (SIZE_LINE - 1))){
			lbuf[len++] = c;
			c_putch(c);
		}
	}
	newline();
	lbuf[len] = 0; // Put NULL

	if(len > 0){
		while(c_isspace(lbuf[--len])); // Skip space
		lbuf[++len] = 0; // Put NULL
	}
}

// Print numeric specified columns
void putnum(short value, short d){
	unsigned char i;
	unsigned char sign;

	if(value < 0){
		sign = 1;
		value = -value;
	} else {
		sign = 0;
	}

	lbuf[6] = 0;
	i = 6;
	do {
		lbuf[--i] = (value % 10) + '0';
		value /= 10;
	} while(value > 0);

	if(sign)
		lbuf[--i] = '-';

	//String length = 6 - i
	while(6 - i < d){ // If short
		c_putch(' '); // Fill space
		d--;
	}
	c_puts(&lbuf[i]);
}

// Input numeric and return value
// Called by only INPUT statement
short getnum(){
	short value, tmp;
	char c;
	unsigned char len;
	unsigned char sign;

	len = 0;
	while((c = c_getch()) != KEY_ENTER){
		if(((c == 8) || (c == 127)) && (len > 0)){ // Backspace manipulation
			len--;
			c_putch(8); c_putch(' '); c_putch(8);
		} else
		if( (len == 0 && (c == '+' || c == '-')) ||
			(len < 6 && c_isdigit(c))){ // Numeric or sign only
			lbuf[len++] = c;
			c_putch(c);
		}
	}
	newline();
	lbuf[len] = 0;

	switch(lbuf[0]){
	case '-':
		sign = 1;
		len = 1;
		break;
	case '+':
		sign = 0;
		len = 1;
		break;
	default:
		sign = 0;
		len = 0;
		break;
	}

	value = 0; // Initialize value
	tmp = 0; // Temp value
	while(lbuf[len]){
		tmp = 10 * value + lbuf[len++] - '0';
		if(value > tmp){ // It means overflow
			err = ERR_VOF;
		}
		value = tmp;
	}
	if(sign)
		return -value;
	return value;
}

// Convert token to i-code
// Return byte length or 0
unsigned char toktoi() {
	unsigned char i; // Loop counter(i-code sometime)
	unsigned char len = 0; //byte counter
	char* pkw = 0; // Temporary keyword pointer
	char* ptok; // Temporary token pointer
	char* s = lbuf; // Pointer to charactor at line buffer
	char c; // Surround the string character, " or '
	short value; //numeric
	short tmp; //numeric for overflow check

	while (*s) {
		while (c_isspace(*s)) s++; // Skip space

		//Try keyword conversion
		for (i = 0; i < SIZE_KWTBL; i++) {
			pkw = (char *)kwtbl[i]; // Point keyword
			ptok = s; // Point top of command line

			// Compare 1 keyword
			while ((*pkw != 0) && (*pkw == c_toupper(*ptok))) {
				pkw++;
				ptok++;
			}

			if (*pkw == 0) {// Case success

				if (len >= SIZE_IBUF - 1) {// List area full
					err = ERR_IBUFOF;
					return 0;
				}

				// i have i-code
				ibuf[len++] = i;
				s = ptok;
				break;
			}
		}

		// Case statement needs an argument except numeric, valiable, or strings
		if(i == I_REM) {
			while (c_isspace(*s)) s++; // Skip space
			ptok = s;
			for (i = 0; *ptok++; i++); // Get length
			if (len >= SIZE_IBUF - 2 - i) {
				err = ERR_IBUFOF;
				return 0;
			}
			ibuf[len++] = i; // Put length
			while (i--) { // Copy strings
				ibuf[len++] = *s++;
			}
			break;
		}

		if (*pkw == 0)
			continue;

		ptok = s; // Point top of command line

		// Try numeric conversion
		if (c_isdigit(*ptok)) {
			value = 0;
			tmp = 0;
			do {
				tmp = 10 * value + *ptok++ - '0';
				if (value > tmp) {
					err = ERR_VOF;
					return 0;
				}
				value = tmp;
			} while (c_isdigit(*ptok));

			if (len >= SIZE_IBUF - 3) {
				err = ERR_IBUFOF;
				return 0;
			}
			s = ptok;
			ibuf[len++] = I_NUM;
			ibuf[len++] = value & 255;
			ibuf[len++] = value >> 8;
		}
		else

		// Try string conversion
		if (*s == '\"' || *s == '\'') {// If start of string
			c = *s++;
			ptok = s;
			for (i = 0; (*ptok != c) && c_isprint(*ptok); i++) // Get length
				ptok++;
			if (len >= SIZE_IBUF - 1 - i) { // List area full
				err = ERR_IBUFOF;
				return 0;
			}
			ibuf[len++] = I_STR; // Put i-code
			ibuf[len++] = i; // Put length
			while (i--) { // Put string
				ibuf[len++] = *s++;
			}
			if (*s == c) s++; // Skip " or '
		}
		else

		// Try valiable conversion
		if (c_isalpha(*ptok)) {
			if (len >= SIZE_IBUF - 2) {
				err = ERR_IBUFOF;
				return 0;
			}
			if (len >= 4 && ibuf[len - 2] == I_VAR && ibuf[len - 4] == I_VAR) { // Case series of variables
				err = ERR_SYNTAX; // Syntax error
				return 0;
			}
			ibuf[len++] = I_VAR; // Put i-code
			ibuf[len++] = c_toupper(*ptok) - 'A'; // Put index of valiable area
			s++;
		}
		else

		// Nothing mutch
		{
			err = ERR_SYNTAX;
			return 0;
		}
	}
	ibuf[len++] = I_EOL; // Put end of line
	return len; // Return byte length
}

// Get line numbere by line pointer
short getlineno(unsigned char *lp) {
	if(*lp == 0) //end of list
		return 32767;// max line bumber
	return *(lp + 1) | *(lp + 2) << 8;
}

// Search line by line number
unsigned char* getlp(short lineno) {
	unsigned char *lp;

	for (lp = listbuf; *lp; lp += *lp)
		if (getlineno(lp) >= lineno)
			break;
	return lp;
}

// Return free memory size
short getsize() {
	unsigned char* lp;

	for (lp = listbuf; *lp; lp += *lp);
	return listbuf + SIZE_LIST - lp - 1;
}

// Insert i-code to the list
// Preconditions to do *ibuf = len
void inslist() {
	unsigned char *insp;
	unsigned char *p1, *p2;
	short len;

	if (getsize() < *ibuf) {
		err = ERR_LBUFOF; // List buffer overflow
		return;
	}

	insp = getlp(getlineno(ibuf));

	if (getlineno(insp) == getlineno(ibuf)) {// line number agree
		p1 = insp;
		p2 = p1 + *p1;
		while ((len = *p2) != 0) {
			while (len--)
				*p1++ = *p2++;
		}
		*p1 = 0;
	}

	// Case line number only
	if (*ibuf == 4)
		return;

	// Make space
	for (p1 = insp; *p1; p1 += *p1);
	len = p1 - insp + 1;
	p2 = p1 + *ibuf;
	while (len--)
		*p2-- = *p1--;

	// Insert
	len = *ibuf;
	p1 = insp;
	p2 = ibuf;
	while (len--)
		*p1++ = *p2++;
}

//Listing 1 line of i-code
void putlist(unsigned char* ip) {
	unsigned char i;

	while (*ip != I_EOL) {
		// Case keyword
		if (*ip < SIZE_KWTBL) {
			c_puts(kwtbl[*ip]);
			if (!nospacea(*ip))
				c_putch(' ');
			if (*ip == I_REM) {
				ip++;
				i = *ip++;
				while (i--) {
					c_putch(*ip++);
				}
				return;
			}
			ip++;
		}
		else

		// Case numeric
		if (*ip == I_NUM) {
			ip++;
			putnum(*ip | *(ip + 1) << 8, 0);
			ip += 2;
			if (!nospaceb(*ip)) c_putch(' ');
		}
		else

		// Case variable
		if (*ip == I_VAR) {
			ip++;
			c_putch(*ip++ + 'A');
			if (!nospaceb(*ip)) c_putch(' ');
		}
		else

		// Case string
		if (*ip == I_STR) {
			char c;

			c = '\"';
			ip++;
			for (i = *ip; i; i--)
				if (*(ip + i) == '\"') {
					c = '\'';
					break;
				}

			c_putch(c);
			i = *ip++;
			while (i--) {
				c_putch(*ip++);
			}
			c_putch(c);
			if (*ip == I_VAR)
				c_putch(' ');
		}

		// Nothing match, I think, such case is impossible
		else {
			err = ERR_SYS;
			return;
		}
	}
}

// Get argument in parenthesis
short getparam(){
	short value;

	if(*cip != I_OPEN){
		err = ERR_PAREN;
		return 0;
	}
	cip++;
	value = iexp();
	if(err) return 0;

	if(*cip != I_CLOSE){
		err = ERR_PAREN;
		return 0;
	}
	cip++;

	return value;
}

// Get value
short ivalue() {
	short value;

	switch (*cip) {
	case I_NUM:
		cip++;
		value = *cip | *(cip + 1) << 8;
		cip += 2;
		break;
	case I_PLUS:
		cip++;
		value = ivalue();
		break;
	case I_MINUS:
		cip++;
		value = 0 - ivalue();
		break;
	case I_VAR:
		cip++;
		value = var[*cip++];
		break;
	case I_OPEN:
		value = getparam();
		break;
	case I_ARRAY:
		cip++;
		value = getparam();
		if (err)
			break;
		if (value >= SIZE_ARRY) {
			err = ERR_SOR;
			break;
		}
		value = arr[value];
		break;
	case I_RND:
		cip++;
		value = getparam();
		if (err)
			break;
		value = getrnd(value);
		break;
	case I_ABS:
		cip++;
		value = getparam();
		if (err)
			break;
		if (value < 0)
			value *= -1;
		break;
	case I_SIZE:
		cip++;
		if ((*cip != I_OPEN) || (*(cip + 1) != I_CLOSE)) {
			err = ERR_PAREN;
			break;
		}
		cip += 2;
		value = getsize();
		break;

	default:
		err = ERR_SYNTAX;
		break;
	}
	return value;
}

// multiply or divide calculation
short imul() {
	short value, tmp;

	value = ivalue();
	if (err)
		return -1;

	while (1)
		switch (*cip) {
		case I_MUL:
			cip++;
			tmp = ivalue();
			value *= tmp;
			break;
		case I_DIV:
			cip++;
			tmp = ivalue();
			if (tmp == 0) {
				err = ERR_DIVBY0;
				return -1;
			}
			value /= tmp;
			break;
		default:
			return value;
		}
}

// add or subtract calculation
short iplus() {
	short value, tmp;

	value = imul();
	if (err)
		return -1;

	while (1)
		switch (*cip) {
		case I_PLUS:
			cip++;
			tmp = imul();
			value += tmp;
			break;
		case I_MINUS:
			cip++;
			tmp = imul();
			value -= tmp;
			break;
		default:
			return value;
		}
}

// The parser
short iexp() {
	short value, tmp;

	value = iplus();
	if (err)
		return -1;

	// conditional expression 
	while (1)
		switch (*cip) {
		case I_EQ:
			cip++;
			tmp = iplus();
			value = (value == tmp);
			break;
		case I_SHARP:
			cip++;
			tmp = iplus();
			value = (value != tmp);
			break;
		case I_LT:
			cip++;
			tmp = iplus();
			value = (value < tmp);
			break;
		case I_LTE:
			cip++;
			tmp = iplus();
			value = (value <= tmp);
			break;
		case I_GT:
			cip++;
			tmp = iplus();
			value = (value > tmp);
			break;
		case I_GTE:
			cip++;
			tmp = iplus();
			value = (value >= tmp);
			break;
		default:
			return value;
		}
}

// PRINT handler
void iprint() {
	short value;
	short len;
	unsigned char i;

	len = 0;
	while (*cip != I_SEMI && *cip != I_EOL) {
		switch (*cip) {
		case I_STR:
			cip++;
			i = *cip++;
			while (i--)
				c_putch(*cip++);
			break;
		case I_SHARP:
			cip++;
			len = iexp();
			if (err)
				return;
			break;
		default:
			value = iexp();
			if (err)
				return;
			putnum(value, len);
			break;
		}

		if (*cip == I_COMMA) {
			cip++;
			if (*cip == I_SEMI || *cip == I_EOL)
				return;
		}
		else {
			if (*cip != I_SEMI && *cip != I_EOL) {
				err = ERR_SYNTAX;
				return;
			}
		}
	}
	newline();
}

// INPUT handler
void iinput() {
	short value;
	short index;
	unsigned char i;
	unsigned char prompt;

	while (1) {
		prompt = 1;

		if (*cip == I_STR) {
			cip++;
			i = *cip++;
			while (i--)
				c_putch(*cip++);
			prompt = 0;
		}

		switch (*cip) {
		case I_VAR:
			cip++;
			if (prompt) {
				c_putch(*cip + 'A');
				c_putch(':');
			}
			value = getnum();
			if (err)
				return;
			var[*cip++] = value;
			break;
		case I_ARRAY:
			cip++;
			index = getparam();
			if (err)
				return;
			if (index >= SIZE_ARRY) {
				err = ERR_SOR;
				return;
			}
			if (prompt) {
				c_puts("@(");
				putnum(index, 0);
				c_puts("):");
			}
			value = getnum();
			if (err)
				return;
			arr[index] = value;
			break;
		default:
			err = ERR_SYNTAX;
			return;
		}

		switch (*cip) {
		case I_COMMA:
			cip++;
			break;
		case I_SEMI:
		case I_EOL:
			return;
		default:
			err = ERR_SYNTAX;
			return;
		}
	}
}

// Variable assignment handler
void ivar() {
	short value;
	short index;

	index = *cip++;
	if (*cip != I_EQ) {
		err = ERR_VWOEQ;
		return;
	}
	cip++;

	value = iexp();
	if (err)
		return;

	var[index] = value;
}

// Array assignment handler
void iarray() {
	short value;
	short index;

	index = getparam();
	if (err)
		return;

	if (index >= SIZE_ARRY) {
		err = ERR_SOR;
		return;
	}

	if (*cip != I_EQ) {
		err = ERR_VWOEQ;
		return;
	}
	cip++;

	value = iexp();
	if (err)
		return;

	arr[index] = value;
}

// LET handler
void ilet() {
	switch (*cip) {
	case I_VAR:
		cip++;
		ivar(); // Variable assignment
		break;
	case I_ARRAY:
		cip++;
		iarray(); // Array assignment
		break;
	default:
		err = ERR_LETWOV;
		break;
	}
}

// Execute a series of i-code
unsigned char* iexe() {
	short lineno; //line number
	unsigned char* lp; //temporary line pointer
	short index, vto, vstep; // FOR-NEXT items
	short condition; //IF condition

	while (*cip != I_EOL) {

		if (c_kbhit()) // check keyin
			if (c_getch() == 27) { // ESC ?
				err = ERR_ESC;
				return NULL;
			}

		switch (*cip) {

		case I_GOTO:
			cip++;
			lineno = iexp(); // get line number
			if (err)
				break;
			lp = getlp(lineno); // search line
			if (lineno != getlineno(lp)) { // if not found
				err = ERR_ULN;
				break;
			}

			clp = lp; // update line pointer
			cip = clp + 3; // update i-code pointer
			break;

		case I_GOSUB:
			cip++;
			lineno = iexp(); // get line number
			if (err)
				break;
			lp = getlp(lineno); // search line
			if (lineno != getlineno(lp)) { // if not found
				err = ERR_ULN;
				break;
			}

			// push pointers
			if (gstki > SIZE_GSTK - 2) { // stack overflow ?
				err = ERR_GSTKOF;
				break;
			}
			gstk[gstki++] = clp; // push line pointer
			gstk[gstki++] = cip; // push i-code pointer

			clp = lp; // update line pointer
			cip = clp + 3; // update i-code pointer
			break;

		case I_RETURN:
			if (gstki < 2) { // stack empty ?
				err = ERR_GSTKUF;
				break;
			}
			cip = gstk[--gstki]; // pop line pointer
			clp = gstk[--gstki]; // pop i-code pointer
			break;

		case I_FOR:
			cip++;

			if (*cip++ != I_VAR) { // no variable
				err = ERR_FORWOV;
				break;
			}

			index = *cip; // get variable index
			ivar(); // var = value
			if (err)
				break;

			if (*cip == I_TO) {
				cip++;
				vto = iexp(); // get TO value
			}
			else {
				err = ERR_FORWOTO;
				break;
			}

			if (*cip == I_STEP) {
				cip++;
				vstep = iexp(); // get STEP value
			}
			else
				vstep = 1; // default STEP value

						   // overflow check
			if (((vstep < 0) && (-32767 - vstep > vto)) ||
				((vstep > 0) && (32767 - vstep < vto))) {
				err = ERR_VOF;
				break;
			}

			// push pointers
			if (lstki > SIZE_LSTK - 5) { // stack overflow ?
				err = ERR_LSTKOF;
				break;
			}
			lstk[lstki++] = clp; // push line pointer
			lstk[lstki++] = cip; // push i-code pointer
								 //Special thanks hardyboy
			lstk[lstki++] = (unsigned char*)(uintptr_t)vto; // push TO value
			lstk[lstki++] = (unsigned char*)(uintptr_t)vstep; // push STEP value
			lstk[lstki++] = (unsigned char*)(uintptr_t)index; // push variable index
			break;

		case I_NEXT:
			cip++;

			if (lstki < 5) { // stack empty ?
				err = ERR_LSTKUF;
				break;
			}

			index = (short)(uintptr_t)lstk[lstki - 1]; // read variable index
			if (*cip++ != I_VAR) { // no variable
				err = ERR_NEXTWOV;
				break;
			}
			if (*cip++ != index) { // not equal index
				err = ERR_NEXTUM;
				break;
			}

			vstep = (short)(uintptr_t)lstk[lstki - 2]; // read STEP value
			var[index] += vstep; // update loop counter
			vto = (short)(uintptr_t)lstk[lstki - 3]; // read TO value

													 // loop end
			if (((vstep < 0) && (var[index] < vto)) ||
				((vstep > 0) && (var[index] > vto))) {
				lstki -= 5; // resume stack
				break;
			}

			// loop continue
			cip = lstk[lstki - 4]; // read line pointer
			clp = lstk[lstki - 5]; // read i-code pointer
			break;

		case I_IF:
			cip++;
			condition = iexp(); // get condition
			if (err) {
				err = ERR_IFWOC;
				break;
			}
			if (condition) // if true continue
				break;
			// If false, same as REM

		case I_REM:
			// Seek pointer to I_EOL
			// No problem even if it points not realy end of line
			while (*cip != I_EOL)
				cip++; // seek end of line
			break;

		case I_STOP:
			while (*clp)
				clp += *clp; // seek end
			return clp;

		case I_VAR:
			cip++;
			ivar();
			break;
		case I_ARRAY:
			cip++;
			iarray();
			break;
		case I_LET:
			cip++;
			ilet();
			break;
		case I_PRINT:
			cip++;
			iprint();
			break;
		case I_INPUT:
			cip++;
			iinput();
			break;

		case I_SEMI:
			cip++;
			break;

		case I_LIST:
		case I_NEW:
		case I_RUN:
			err = ERR_COM;
			break;

		default:
			err = ERR_SYNTAX;
			break;
		}

		if (err)
			return NULL;
	}
	return clp + *clp;
}

// RUN command handler
void irun() {
	unsigned char* lp;

	gstki = 0;
	lstki = 0;
	clp = listbuf;

	while (*clp) {
		cip = clp + 3;
		lp = iexe();
		if (err)
			return;
		clp = lp;
	}
}

// LIST command handler
void ilist() {
	short lineno;

	lineno = (*cip == I_NUM) ? getlineno(cip) : 0;

	for (clp = listbuf;
	*clp && (getlineno(clp) < lineno);
		clp += *clp);

		while (*clp) {
			putnum(getlineno(clp), 0);
			c_putch(' ');
			putlist(clp + 3);
			if (err)
				break;
			newline();
			clp += *clp;
		}
}

//NEW command handler
void inew(void) {
	unsigned char i;

	for (i = 0; i < 26; i++)
		var[i] = 0;
	for (i = 0; i < SIZE_ARRY; i++)
		arr[i] = 0;
	gstki = 0;
	lstki = 0;
	*listbuf = 0;
	clp = listbuf;
}

//Command precessor
void icom() {
	cip = ibuf;
	switch (*cip) {
	case I_NEW:
		cip++;
		if (*cip == I_EOL)
			inew();
		else
			err = ERR_SYNTAX;
		break;
	case I_LIST:
		cip++;
		if (*cip == I_EOL || *(cip + 3) == I_EOL)
			ilist();
		else
			err = ERR_SYNTAX;
		break;
	case I_RUN:
		cip++;
		irun();
		break;
	default:
		iexe();
		break;
	}
}

// Print OK or error message
void error() {
	if (err) {
		if (cip >= listbuf && cip < listbuf + SIZE_LIST && *clp)
		{
			newline();
			c_puts("LINE:");
			putnum(getlineno(clp), 0);
			c_putch(' ');
			putlist(clp + 3);
		}
		else
		{
			newline();
			c_puts("YOU TYPE: ");
			c_puts(lbuf);
		}
	}

	newline();
	c_puts(errmsg[err]);
	newline();
	err = 0;
}

/*
TOYOSHIKI Tiny BASIC
The BASIC entry point
*/
void basic(){
	unsigned char len;

	inew();
	c_puts("TOYOSHIKI TINY BASIC"); newline();
	c_puts(STR_EDITION);
	c_puts(" EDITION"); newline();
	error(); // Print OK, and Clear error flag

	// Input 1 line and execute
	while(1){
		c_putch('>');// Prompt
		c_gets(); // Input 1 line
		len = toktoi(); // Convert token to i-code
		if(err){ // Error
			error();
			continue; // Do nothing
		}

		if(*ibuf == I_SYSTEM){
			return;
		}

		if(*ibuf == I_NUM){ // Case the top includes line number
			*ibuf = len; // Change I_NUM to byte length
			inslist(); // Insert list
			if (err) // Error
				error();  // Print error message
			continue;
		}

		icom(); // Execute direct
		error(); // Print OK, and Clear error flag
	}
}
