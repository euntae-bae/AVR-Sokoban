/*
* _glcd.h
*
* Created: 2015-12-16 오후 7:10:43
*  Author: SongYi Jung
*/

void cmndl(byte cmd);	/* lcd 명령 출력 */
void cmndr(byte cmd);
void cmnda(byte cmd);

void datal(byte dat);	/* 1 문자 출력  */
void datar(byte dat);
void dataa(byte dat) ;

void lcd_clear(void);	/* GLCD Clear */
void lcd_init(void);	/* GLCD Initialize */
void lcd_xy(byte x, byte y );	/* 문자 위치 세트 */
void lcd_char(byte character);	/* 한 문자 출력 */
void lcd_string(byte x,byte y,char *string);	/* 문자열 출력 */

void GLCD_Axis_xy(unsigned char x, unsigned char y);
void GLCD_Dot(unsigned char xx,unsigned char y);	// 점을 그립니다.
void ScreenBuffer_clear(void);
void ScreenBuffer_clear_part(unsigned char i);
void GLCD_Line(unsigned char x1,unsigned char y1,unsigned char x2,unsigned char y2);	// 라인을 그립니다
void GLCD_Rectangle(unsigned char x1,unsigned char y1,unsigned char x2,unsigned char y2);	// 직사각형을 그립니다
void GLCD_Rectangle_black(unsigned char x1,unsigned char y1,unsigned char x2,unsigned char y2);	//직사각형을 그리고 안을 색칠합니다
void GLCD_Circle(unsigned char x1,unsigned char y1,unsigned char r); // 원을 그립니다.
void GLCD_Circle_black(unsigned char x1,unsigned char y1,unsigned char r); //원을 그리고 안을 색칠합니다

unsigned char GLCD_1DigitDecimal(unsigned char number, unsigned char flag);	// 1자리의 10진수 값을 표시합니다.
void GLCD_2DigitDecimal(unsigned char number);	// 2자리의 10진수 값을 표시합니다.
void GLCD_3DigitDecimal(unsigned int number);	// 3자리의 10진수 값을 표시합니다.
void GLCD_4DigitDecimal(unsigned int number);	// 4자리의 10진수 값을 표시합니다.

void glcd_draw_bitmap(const byte *bmp, byte x, byte y, byte dx, byte dy);
void glcd_draw_bitmap_P(const byte *bmp, byte x, byte y, byte dx, byte dy);