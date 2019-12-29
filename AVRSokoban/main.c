/************************************
 * AVR Sokoban
 * main.c
 * Programmed by: 12161594 배은태
 * Last Modified: 2019-12-18
 ************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 14745600UL
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_main.h"
#include "_glcd.h"
#include "DataAVR.h"
// 음표 데이터
#include "data/bgm/sml.h"	
#include "data/bgm/clear.h"
#include "data/bgm/fail.h"
#include "data/bgm/ending.h"
#include "data/bgm/life.h"
#include "data/bgm/life2.h"
#include "data/bgm/smb.h"
// 게임 스프라이트 데이터
#include "data/sprite.h"
// 스테이지 데이터	
#include "data/stage.h"		

/* 매크로 상수 및 자료형 재정의 */
#define REVERSE_CONTROLLER_MODE	1	// 조이스틱 방향과 보드 방향이 불일치하는 경우 이 상수를 1로 설정하여 입력값을 보정함

#define BUZ_PORT	PORTG
#define BUZ_ON		0x10
#define BUZ_OFF		0x00

typedef byte bool;
#define true	1
#define false	0

#define DEFAULT_LIFE	3

/* 전역변수 */
// 게임 상태를 표현하기 위한 열거 상수(유한상태기계를 이용한 게임 화면 전이)
enum GameStatus {
	GAME_INIT = 0,
	GAME_INTRO,
	GAME_START_MENU,
	GAME_STAGE_INTRO,
	GAME_STAGE,
	GAME_STAGE_CLEAR,
	GAME_STAGE_FAILED,
	GAME_GAMECLEAR,
	GAME_GAMEOVER
};

// 조이스틱 입력 방향
enum Direction {
	DIR_NONE = 0,
	DIR_LEFT,
	DIR_RIGHT,
	DIR_UP,
	DIR_DOWN
};

// 게임 데이터 관리를 위한 자료구조 정의
typedef struct _GameData {
	byte gameStatus;			// 현재 게임 상태
	byte stage;					// 현재 스테이지
	byte life;					// 플레이어의 생명(리셋 가능 횟수)
	byte prevDirX, prevDirY;	// 이전 프레임에서의 조이스틱 입력 방향
	byte curDirX, curDirY;		// 현재 조이스틱 입력 방향
	byte playerX, playerY;		// 스테이지상의 플레이어 위치(8x8 (스프라이트)블록 단위)
	bool playerInGoal;			// 플레이어가 목적지 위에 서있는지 여부
	char *curStageBuffer;		// 화면에 출력될 현재 스테이지(맵) 데이터
	char *dupStageBuffer;		// 비교를 위한 스테이지 데이터 원본
	byte goalCount;				// 현재 스테이지에서 남은 목적지 개수
	byte stageWidth, stageHeight;
} GameData;

volatile GameData gameData;

// 음원 재생 상태를 표현하기 위한 열거 상수
enum AudioStatus {
	AUDIO_STOP = 0,
	AUDIO_PLAY,
};

// 음원 리스트에 쉽게 접근할 수 있도록 audioList의 순서에 대응되는 열거 상수 정의
enum AudioID {
	BGM_SML = 0,
	ME_CLEAR,
	ME_FAIL,
	BGM_ENDING,
	SE_LIFE,
	SE_LIFE2,
	ME_SMB
};

// 음원 리스트
volatile const NoteDataAVR *audioList[] = { sml, clear, fail, ending, selife, selife2, smb };

// 각 음원의 길이(audioList의 순서에 대응)
unsigned int noteSize[] = { 113, 11, 20, 57, 14, 14, 14 };

// 각 음원의 루프 지점(인덱스), (audioList의 순서에 대응)
volatile const unsigned int loopPoints[] = { 10, 0, 0, 0, 0, 0, 0 };

volatile byte audioStatus = AUDIO_STOP; // 오디오 재생 상태
volatile NoteDataAVR curNote; // 현재 재생될 음표, 프로그램 메모리에서 memcpy_P 함수를 통해 읽어들인다.
volatile byte curOvfCnt = 0; // 현재 음표에서 타이머 오버플로우 발생 횟수
volatile bool compLoop = false; // 한 음표의 재생을 위한 타이머 오버플로우 반복 완료 여부
volatile bool buzStatus = 0; // 부저의 On/Off

/* 함수 프로토타입 */
void initPort(void);
void initExternINT(void);
void initTimer(void);
void initADC(void);
void initDevice(void);
unsigned int readADCData(byte adcInput);

void playAudio(unsigned int id, unsigned int length);
void playAudioLoop(unsigned int id, unsigned int length, unsigned int loopPoint);
void stopAudio(void);

void gameInit(void);
void gameIntro(void);
void gameStartMenu(void);
void gameStageIntro(void);
void gameStage(void);
void gameStageClear(void);					// 스테이지 클리어 시 호출
void gameStageFailed(void);					// 리셋 버튼을 눌렀을 때 호출
void gameGameclear(void);
void gameGameover(void);

byte getDirX(unsigned int adcData);			// ADC3 데이터로부터 조이스틱 방향을 얻어낸다.
byte getDirY(unsigned int adcData);			// ADC4 데이터로부터 조이스틱 방향을 얻어낸다.
void loadStage(void);						// 프로그램 메모리에 저장된 각 스테이지의 맵 데이터를 gameData.curStageBuffer에 복사한다.
void loadPlayerXY(void);					// gameData.curStageBuffer로부터 플레이어의 X, Y 좌표(8x8 픽셀 블럭 단위)를 찾는다.
void loadGoalCount(void);					// gameData.curStageBuffer로부터 목적지의 개수를 얻는다.
unsigned int xyToIdx(byte x, byte y);		// x, y 좌표를 입력하면 선형 배열의 인덱스를 반환한다. curStageBuffer는 선형 배열임에 주의
char getObject(byte x, byte y);				// x, y 좌표에 위치한 물체의 종류를 반환한다.
char getSrcObject(byte x, byte y);			// gameData.dupStageBuffer의 x, y 좌표에 위치한 물체의 종류를 반환한다.
void setObject(byte x, byte y, char obj);	// 지정한 좌표에 특정 물체를 배치한다.
void drawStage(void);						// 스테이지 렌더링(화면에 출력될 정보가 갱신됐을 경우에만 호출)

/* ISR */
ISR(INT0_vect)
{
	if (gameData.gameStatus == GAME_START_MENU) {
		stopAudio();
		gameData.gameStatus = GAME_STAGE_INTRO;
	}
	if (gameData.gameStatus == GAME_STAGE) {
		gameData.gameStatus = GAME_STAGE_FAILED;
	}
	if (gameData.gameStatus == GAME_GAMECLEAR) {
		gameInit();
		gameData.gameStatus = GAME_START_MENU;
	}
}

// 부저의 진동을 제어
// 음 길이만큼 음악을 재생하는 것은 다른 함수에서 처리해야 한다.
ISR(TIMER2_OVF_vect)
{
	// loopCnt가 음수이면 쉼표이므로 진동이 필요 없다.
	if (curNote.loopCnt == 0) {
		buzStatus = !buzStatus;
		if (buzStatus) {
			BUZ_PORT = BUZ_ON;
		}
		else {
			BUZ_PORT = BUZ_OFF;
		}
		TCNT2 = curNote.remainCnt;
	}
	else if (curNote.loopCnt > 0) {
		curOvfCnt++;

		if (compLoop) {
			compLoop = false;
			buzStatus = !buzStatus;
			if (buzStatus) {
				BUZ_PORT = BUZ_ON;
			}
			else {
				BUZ_PORT = BUZ_OFF;
			}
			curOvfCnt = 0;
		}
		else if (curOvfCnt >= curNote.loopCnt) {
			compLoop = true;
			TCNT2 = curNote.remainCnt;
		}
	}
}

void initPort(void)
{
	PORTA = 0x00; DDRA = 0xff;
	PORTB = 0xff; DDRB = 0xff;
	PORTC = 0x00; DDRC = 0xf0;
	PORTD = 0x80; DDRD = 0x80;
	PORTE = 0x00; DDRE = 0xff;
	PORTF = 0x00; DDRF = 0x00;
	PORTG = 0x00; DDRG = 0x1f; 
}

void initExternINT(void)
{
	EIMSK = 0x01; // 0000 1111
	EICRA = 0x03; // 1111 1111
}

void initTimer(void)
{
	// 타이머/카운터0: 메인 타이머
	TCCR0 = 0x00; // 수정할 것
	TCNT0 = 0;
	// 타이머/카운터2: 부저 출력 제어용 타이머
	TCCR2 = 0;
	TCNT2 = 0;
	TIMSK = 0b01000001;
}

void initADC(void)
{
	ADCSRA = 0x00;
	ADMUX = 0x00;
	ACSR = 0x80;
	ADCSRA = 0xc3; // 1100 0011
}

void initDevice(void)
{
	cli();
	initPort();
	initExternINT();
	initTimer();
	initADC();
	lcd_init();
	lcd_clear();
	ScreenBuffer_clear();
	sei();
}

// ADC 데이터를 읽어서 반환하는 함수
unsigned int readADCData(byte adcInput)
{
	unsigned int adc = 0;
	ADCSRA = 0x83; // Conversion time: 6uS, ADC 분주비: 8
	ADMUX = 0x40 | adcInput;
	ADCSRA |= 0x40; // A/D 변환 시작
	while (!(ADCSRA & 0x10)); // ADIF=1일 때까지 대기
	adc += ADCL + (ADCH << 8);
	ADCSRA = 0x00;
	return adc;
}

// 지정한 음원을 1회 출력하는 함수
void playAudio(unsigned int id, unsigned int length)
{
	volatile const NoteDataAVR *curMusic = audioList[id];
	unsigned int curNoteIdx = 0;
	TCCR2 = 0x03;
	audioStatus = AUDIO_PLAY;
	
	while (curNoteIdx < length) {
		if (audioStatus == AUDIO_STOP)
			return;
		// 전역변수 curNote에 프로그램 메모리에 정의된 음원 데이터를 적재한다.
		memcpy_P((void*)&curNote, (const void*)(curMusic + curNoteIdx), sizeof(NoteDataAVR));
		if (curNote.loopCnt == 0) // 타이머 오버플로우가 발생하지 않는 음표
			TCNT2 = curNote.remainCnt;
		_delay_ms(curNote.length); // 음길이만큼 음을 지속하기 위해 지연 함수 호출
		curNoteIdx++;
	}
	TCCR2 = 0x00;
	audioStatus = AUDIO_STOP;
}

// 지정한 음원을 계속 출력하는 함수, loopPoint를 지정하면 음악이 끝나는 시점에 다시 loopPoint부터 시작한다.
void playAudioLoop(unsigned int id, unsigned int length, unsigned int loopPoint)
{
	volatile const NoteDataAVR *curMusic = audioList[id];
	unsigned int curNoteIdx = 0;
	TCCR2 = 0x03;
	audioStatus = AUDIO_PLAY;
	
	while (1) {
		if (audioStatus == AUDIO_STOP)
			return;
		if (curNoteIdx >= length)
			curNoteIdx = loopPoint;
		memcpy_P((void*)&curNote, (const void*)(curMusic + curNoteIdx), sizeof(NoteDataAVR));
		if (curNote.loopCnt == 0)
			TCNT2 = curNote.remainCnt;
		_delay_ms(curNote.length);
		curNoteIdx++;
	}
}

void stopAudio(void)
{
	TCCR2 = 0; // 부저 출력용 타이머의 클럭을 차단한다 -> 카운트 중지(인터럽트 발생도 중지)
	audioStatus = AUDIO_STOP;
}

void gameInit(void)
{
	initDevice();
	audioStatus = AUDIO_STOP;
	gameData.gameStatus = GAME_INIT;
	gameData.stage = 1;
	gameData.life = DEFAULT_LIFE;
	gameData.prevDirX = DIR_NONE;
	gameData.prevDirY = DIR_NONE;
	gameData.curDirX = DIR_NONE;
	gameData.curDirY = DIR_NONE;
	if (gameData.curStageBuffer) {
		free(gameData.curStageBuffer);
		gameData.curStageBuffer = NULL;
	}
	if (gameData.dupStageBuffer) {
		free(gameData.dupStageBuffer);
		gameData.dupStageBuffer = NULL;
	}
}

void gameIntro(void)
{
	gameData.gameStatus = GAME_INTRO;
	lcd_clear();
	ScreenBuffer_clear();
	lcd_string(3, 5, "(C) 2019");
	lcd_string(4, 0, "12161594 Euntae Bae");
	_delay_ms(2000);
	gameData.gameStatus = GAME_START_MENU;
	
	lcd_clear();
	ScreenBuffer_clear();
}

void gameStartMenu(void)
{
	gameData.gameStatus = GAME_START_MENU;
	lcd_string(0, 0, "==============");
	lcd_string(1, 0, "AVR Sokoban");
	lcd_string(2, 0, "==============");
	lcd_string(5, 0, "*Press PD0 to start*"); // TODO: blink 기능 추가
	playAudioLoop(BGM_SML, noteSize[BGM_SML], loopPoints[BGM_SML]);
}

void gameStageIntro(void)
{
	gameData.gameStatus = GAME_STAGE_INTRO;
	char stageStr[20];
	char lifeStr[20];
	
	lcd_clear();
	ScreenBuffer_clear();
	
	sprintf(stageStr, "Stage %d", gameData.stage);
	sprintf(lifeStr, "Life %d", gameData.life);
	lcd_string(3, 7, stageStr);
	lcd_string(4, 7, lifeStr);
	
	loadStage(); // gameData.curStageBuffer에 스테이지 데이터를 불러온다.
	playAudio(ME_SMB, noteSize[ME_SMB]);
	_delay_ms(100);
	lcd_clear();
	ScreenBuffer_clear();
	if (gameData.life > 0) {
		gameData.gameStatus = GAME_STAGE;
		drawStage();
	}
	else
		gameData.gameStatus = GAME_GAMEOVER;
}

//  : space (통과 가능)
// #: wall
// .: goal (통과 가능)
// ^: box
// *: box in goal
// @: player
// $: life (통과 가능): 박스와 겹쳐지면 소멸한다.

// 박스의 속성: 박스 뒤에 한 칸 이상의 통과 가능한 물체가 있으면 밀 수 있다.
// 목적지에 도착한 박스도 일반 박스와 같은 성질을 갖는다.
void gameStage(void)
{
	// 입력 처리, 게임 정보 갱신
	bool updated = false;
	
	/* 게임 오브젝트의 움직임 처리 */
	// TODO: 화면 스크롤
	
	// 입력에서의 X: 좌우, GLCD에서의 Y: 열
	if (gameData.curDirX != gameData.prevDirX) {
		if (gameData.curDirX == DIR_LEFT && gameData.playerY > 0) {
			char lobject = getObject(gameData.playerX, gameData.playerY - 1);	// 플레이어 바로 왼쪽 물체
			char llobject = '#';												// 플레이어 두 칸 왼쪽 물체
			if (gameData.playerY >= 2)
				llobject = getObject(gameData.playerX, gameData.playerY - 2);
				
			if (lobject == ' ') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (lobject == '$') {
				gameData.life++;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
				playAudio(SE_LIFE, noteSize[SE_LIFE]);
			}
			else if (lobject == '*' && (llobject == ' ' || llobject == '.' || llobject == '$')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				if (llobject == '.')
					setObject(gameData.playerX, gameData.playerY - 1, '*');
				else {
					setObject(gameData.playerX, gameData.playerY - 1, '^');
					gameData.goalCount++;
				}
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (lobject == '.') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (lobject == '^' && (llobject == ' ' || llobject == '$')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX, gameData.playerY - 1, '^');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (lobject == '^' && llobject == '.') {
				gameData.goalCount--;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY--;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX, gameData.playerY - 1, '*');
				gameData.playerInGoal = false;
				updated = true;
			}
		}
		
		else if (gameData.curDirX == DIR_RIGHT && gameData.playerY < gameData.stageWidth) {
			char robject = getObject(gameData.playerX, gameData.playerY + 1);	// 플레이어 바로 오른쪽 물체
			char rrobject = '#';												// 플레이어 두 칸 오른쪽 물체
			if (gameData.stageWidth - gameData.playerY > 2)
				rrobject = getObject(gameData.playerX, gameData.playerY + 2);
				
			if (robject == ' ') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (robject == '$') {
				gameData.life++;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				playAudio(SE_LIFE, noteSize[SE_LIFE]);
				updated = true;
			}
			else if (robject == '*' && (rrobject == ' ' || rrobject == '.' || rrobject == '$')) {
				if (gameData.playerInGoal)
				setObject(gameData.playerX, gameData.playerY, '.');
				else
				setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				if (rrobject == '.')
					setObject(gameData.playerX, gameData.playerY + 1, '*');
				else {
					setObject(gameData.playerX, gameData.playerY + 1, '^');
					gameData.goalCount++;
				}
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (robject == '.') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (robject == '^' && (rrobject == ' ' || rrobject == '$')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX, gameData.playerY + 1, '^');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (robject == '^' && rrobject == '.') {
				gameData.goalCount--;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerY++;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX, gameData.playerY + 1, '*');
				gameData.playerInGoal = false;
				updated = true;
			}
		}
	}
	
	// 입력에서의 Y: 상하, GLCD에서의 X: 행
	if (gameData.curDirY != gameData.prevDirY) {
		if (gameData.curDirY == DIR_UP && gameData.playerX > 0) {
			char uobject = getObject(gameData.playerX - 1, gameData.playerY);	// 플레이어 바로 위쪽 물체
			char uuobject = '#';												// 플레이어 두 칸 위쪽 물체
			if (gameData.playerX >= 2)
				uuobject = getObject(gameData.playerX - 2, gameData.playerY);
				
			if (uobject == ' ') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (uobject == '$') {
				gameData.life++;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
				playAudio(SE_LIFE, noteSize[SE_LIFE]);
			}
			else if (uobject == '*' && (uuobject == ' ' || uuobject == '$' || uuobject == '.')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				if (uuobject == '.')
					setObject(gameData.playerX - 1, gameData.playerY, '*');
				else {
					setObject(gameData.playerX - 1, gameData.playerY, '^');
					gameData.goalCount++;
				}
				gameData.playerInGoal = true;
				updated = true;
			}
			if (uobject == '.') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (uobject == '^' && (uuobject == ' ' || uuobject == '$')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX - 1, gameData.playerY, '^');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (uobject == '^' && uuobject == '.') {
				gameData.goalCount--;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX--;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX - 1, gameData.playerY, '*');
				gameData.playerInGoal = false;
				updated = true;
			}
		}
		
		else if (gameData.curDirY == DIR_DOWN && gameData.playerX < gameData.stageHeight) {
			char dobject = getObject(gameData.playerX + 1, gameData.playerY);	// 플레이어 바로 아래쪽 물체
			char ddobject = '#';												// 플레이어 두 칸 아래쪽 물체
			if (gameData.stageHeight - gameData.playerX > 2)
				ddobject = getObject(gameData.playerX + 2, gameData.playerY);
				
			if (dobject == ' ') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (dobject == '$') {
				gameData.life++;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = false;
				updated = true;
				playAudio(SE_LIFE, noteSize[SE_LIFE]);
			}
			else if (dobject == '*' && (ddobject == ' ' || ddobject == '$' || ddobject == '.')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				if (ddobject == '.')
					setObject(gameData.playerX + 1, gameData.playerY, '*');
				else {
					setObject(gameData.playerX + 1, gameData.playerY, '^');
					gameData.goalCount++;
				}
				gameData.playerInGoal = true;
				updated = true;
			}
			if (dobject == '.') {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				gameData.playerInGoal = true;
				updated = true;
			}
			else if (dobject == '^' && (ddobject == ' ' || ddobject == '$')) {
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX + 1, gameData.playerY, '^');
				gameData.playerInGoal = false;
				updated = true;
			}
			else if (dobject == '^' && ddobject == '.') {
				gameData.goalCount--;
				if (gameData.playerInGoal)
					setObject(gameData.playerX, gameData.playerY, '.');
				else
					setObject(gameData.playerX, gameData.playerY, ' ');
				gameData.playerX++;
				setObject(gameData.playerX, gameData.playerY, '@');
				setObject(gameData.playerX + 1, gameData.playerY, '*');
				gameData.playerInGoal = false;
				updated = true;
			}
		}
	}
	// 정보가 갱신됐을 경우 화면을 다시 그린다.
	if (updated) {
		// 디버깅용 코드
		//if (gameData.playerInGoal)
			//PORTB = 0x00;
		//else
			//PORTB = 0xff;
		////////////////////////////
		drawStage();
		if (gameData.goalCount == 0)
			gameData.gameStatus = GAME_STAGE_CLEAR;
	}
}

void gameStageClear(void)
{
	_delay_ms(500);
	lcd_string(3, 2, "Stage Clear!");
	gameData.stage++;
	if (gameData.stage > NUM_OF_STAGE)
		gameData.gameStatus = GAME_GAMECLEAR;
	else
		gameData.gameStatus = GAME_STAGE_INTRO;
	playAudio(ME_CLEAR, noteSize[ME_CLEAR]);
	_delay_ms(100);
}

void gameStageFailed(void)
{
	setObject(gameData.playerX, gameData.playerY, 'x'); // x를 눌러 조의를 표하시오.
	drawStage();
	playAudio(ME_FAIL, noteSize[ME_FAIL]);
	gameData.life--;
	if (gameData.life <= 0)
		gameData.gameStatus = GAME_GAMEOVER;
	else
		gameData.gameStatus = GAME_STAGE_INTRO;
	_delay_ms(100);
}

void gameGameclear(void)
{
	stopAudio();
	ScreenBuffer_clear();
	lcd_clear();
	lcd_string(3, 1, "GameClear");
	lcd_string(4, 1, "Congratulations!");
	lcd_string(5, 1, "PD0 to return menu");
	playAudioLoop(BGM_ENDING, noteSize[BGM_ENDING], loopPoints[BGM_ENDING]);
}

void gameGameover(void)
{
	lcd_clear();
	ScreenBuffer_clear();
	stopAudio();
	lcd_string(3, 1, "GameOver");
	//playAudio(); // 게임 오버 음악 재생
	_delay_ms(2000);
	gameInit();
	gameData.gameStatus = GAME_START_MENU;
	lcd_clear();
	ScreenBuffer_clear();
}

byte getDirX(unsigned int adcData)
{
	if (adcData < 300)
		return DIR_LEFT;
	else if (adcData > 700)
		return DIR_RIGHT;
	return DIR_NONE;
}

byte getDirY(unsigned int adcData)
{
	if (adcData < 300)
		return DIR_UP;
	else if (adcData > 700)
		return DIR_DOWN;
	return DIR_NONE;
}

// 전역 변수 gameData.curStageBuffer에 stageList의 데이터를 복사한다.
void loadStage(void)
{
	byte stgIdx = gameData.stage - 1;
	word stgSize = stageSize[stgIdx].width * stageSize[stgIdx].height;
	gameData.stageWidth = stageSize[stgIdx].width;
	gameData.stageHeight = stageSize[stgIdx].height;
	gameData.playerInGoal = false;
	
	if (stgIdx < 0)
		stgIdx = 0;
	if (gameData.curStageBuffer)
		free(gameData.curStageBuffer);
	if (gameData.dupStageBuffer)
		free(gameData.dupStageBuffer);
		
	gameData.curStageBuffer = (char*)malloc(stgSize);
	gameData.dupStageBuffer = (char*)malloc(stgSize);
	memcpy_P(gameData.curStageBuffer, stageList[stgIdx], stgSize);
	memcpy_P(gameData.dupStageBuffer, stageList[stgIdx], stgSize);
	loadPlayerXY();
	loadGoalCount();
}

void loadPlayerXY(void)
{	
	byte x, y; // x가 행 번호, y가 열 번호임에 주의할 것
	for (x = 0; x <= gameData.stageHeight; x++) {
		for (y = 0; y <= gameData.stageWidth; y++) {
			if (gameData.curStageBuffer[xyToIdx(x, y)] == '@') {
				gameData.playerX = x;
				gameData.playerY = y;
				return;
			}
		}
	}
}

void loadGoalCount(void)
{
	byte x, y;
	gameData.goalCount = 0;
	for (x = 0; x <= gameData.stageHeight; x++) {
		for (y = 0; y <= gameData.stageWidth; y++) {
			if (gameData.curStageBuffer[xyToIdx(x, y)] == '.') {
				gameData.goalCount++;
			}
		}
	}
}

unsigned int xyToIdx(byte x, byte y)
{
	return x * gameData.stageWidth + y;
}

char getObject(byte x, byte y)
{
	return gameData.curStageBuffer[xyToIdx(x, y)];
}

char getSrcObject(byte x, byte y)
{
	return gameData.dupStageBuffer[xyToIdx(x, y)];
}

void setObject(byte x, byte y, char obj)
{
	gameData.curStageBuffer[xyToIdx(x, y)] = obj;
}

void drawStage(void)
{
	byte stgWidth = stageSize[gameData.stage - 1].width;
	byte stgHeight = stageSize[gameData.stage - 1].height;
	
	char stgStr[10];
	char lifeStr[10];
	byte x, y;
	
	lcd_clear();
	ScreenBuffer_clear();
	
	sprintf(stgStr, "LV%02d", gameData.stage);
	sprintf(lifeStr, "LF%02d", gameData.life);
	
	for (x = 0; x < stgHeight; x++) {
		for (y = 0; y < stgWidth; y++) {
			switch (getObject(x, y)) {
			case '#': // wall
				glcd_draw_bitmap_P(sprWall, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case '.': // goal
				glcd_draw_bitmap_P(sprGoal, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case '^': // box
				glcd_draw_bitmap_P(sprBox, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case '*': // box in goal
				glcd_draw_bitmap_P(sprGoalBox, SPRITE_WIDTH *x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case '@': // player
				glcd_draw_bitmap_P(sprPlayer, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case '$': // life(1up)
				glcd_draw_bitmap_P(sprLife, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			case 'x': // player down(on reset)
				glcd_draw_bitmap_P(sprPlayerDown, SPRITE_WIDTH * x, SPRITE_HEIGHT * y, SPRITE_WIDTH, SPRITE_HEIGHT);
				break;
			}
		}
	} // end of for
	
	lcd_string(0, 16, stgStr);
	lcd_string(1, 16, lifeStr);
}

int main(void)
{
	unsigned int dataADC3;
	unsigned int dataADC4;
	
	gameInit();
	gameIntro();
	
	// 게임 메인 루프
    while (1) {
		//lcd_clear();
		//ScreenBuffer_clear();
		// 게임 화면 렌더링: GLCD의 속도가 느리기 때문에 깜빡임을 최소화할 필요가 있다.
		// -> 화면에 출력될 정보가 갱신될 경우에만 그리기
		switch (gameData.gameStatus) {
		case GAME_START_MENU:
			gameStartMenu();
			break;
		case GAME_STAGE_INTRO:
			gameStageIntro();
			break;
		case GAME_STAGE:
			// 사용자 입력
			dataADC3 = readADCData(3); // 좌우 조작
			dataADC4 = readADCData(4); // 상하 조작
#if REVERSE_CONTROLLER_MODE == 1
			gameData.curDirX = getDirX(dataADC4);
			gameData.curDirY = getDirY(dataADC3);
			if (gameData.curDirX == DIR_LEFT)
				gameData.curDirX = DIR_RIGHT;
			else if (gameData.curDirX == DIR_RIGHT)
				gameData.curDirX = DIR_LEFT;
#else
			gameData.curDirX = getDirX(dataADC3);
			gameData.curDirY = getDirY(dataADC4);
#endif
			// 화면 렌더링
			gameStage();
			gameData.prevDirX = gameData.curDirX;
			gameData.prevDirY = gameData.curDirY;
			break;
		case GAME_STAGE_CLEAR:
			gameStageClear();
			break;
		case GAME_STAGE_FAILED:
			gameStageFailed();
			break;
		case GAME_GAMECLEAR:
			gameGameclear();
			break;
		case GAME_GAMEOVER:
			gameGameover();
			break;
		}
		_delay_ms(200);
	}
	return 0;
}