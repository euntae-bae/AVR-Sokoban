/************************************
 * stage.h
 * Programmed by: 12161594 배은태
 * Last Modified: 2019-12-18
 ************************************/
#ifndef STAGE_H_
#define STAGE_H_

#define NUM_OF_STAGE	5

typedef struct _StageSize {
	unsigned char width;
	unsigned char height;
} StageSize;

const StageSize stageSize[] = {
	{8, 8}, { 12, 8 }, { 8, 9 },  { 9, 9 }, { 10, 8 }
};

const char stage1[] PROGMEM =
"  ###   "
"  #.#   "
"  # ####"
"###^ ^.#"
"#. ^@###"
"####^#  "
"   #.#  "
"   ###  ";

const char stage2[] PROGMEM = 
"############"
"###.$    ###"
"######^  ###"
"######   ###"
"###. ^   ###"
"###.  ^  ###"
"###  @   ###"
"############";

const char stage3[] PROGMEM =
"  ##### "
"###   # "
"#.@^  # "
"### ^.# "
"#.##^ # "
"# # . ##"
"#^ ^^^.#"
"#   .  #"
"########";

const char stage4[] PROGMEM = 
"#####    "
"#   #    "
"# ^ # ###"
"# ^ # #.#"
"### ###.#"
" ## @  .#"
" # ^ #  #"
" #   ####"
" #####   ";

const char stage5[] PROGMEM =
"######### "
"#.....  # "
"### ^$# ##"
"  # ^##  #"
"  #^ ^ ^ #"
"  #   #  #"
"  ##  # @#"
"   #######";

const char *stageList[] = {
	stage1, stage2, stage3, stage4, stage5
};

#endif