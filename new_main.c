#define _CRT_SECURE_NO_WARNINGS
#define TEXT_MAX_SIZE 10 // using textlcd 
#define IMAGE_SIZE 32 // 32*32 pixel
#define BUFFER_SIZE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <signal.h>
#include <pthread.h>


#include "led-matrix-c.h" // for matrix controls

//hardware pins
#define LMOVEPIN 28
#define RMOVEPIN 29


typedef struct {
	int delay;
	char pixels[IMAGE_SIZE][IMAGE_SIZE];
}Image;

typedef struct {
	int hasMelody;
	int note_n;
	int* frequency;
	int* duration; // 1 == 16분음표, 4 == 4분음표
}Melody;

typedef struct {
	char name[TEXT_MAX_SIZE];
	int length;
	Melody melody;
	Image* images;
}Animation;

typedef struct {
	int newIndex; // start index pointer
	int isNewData; // 0 if no, 1 if yes
	int lastIndex;
	int anim_n; // 
}History;

typedef struct {
	int currentViewIndex;
	int isActive; // for auto screen off

}MatrixStatus;


/*global variable*/
Animation* animations;
MatrixStatus userMatrix;
History history;
int animation_n;
int animation_index;
int melodyOn;

struct RGBLedMatrix *matrix;
struct LedCanvas *realtime_canvas;


/*method*/
int dataload();
void printData();
void printLED();
void printMelody();
//void init_matrix();
void print_matrix(char image[32][32], struct LedCanvas *canvas, int speed);
void printLEDDefault();
void loop();
void leftShift();
void rightShift();
void initPin();
void interruptTest();
void datasync();
void printLedData();
void printLEDEnd();


// signal handlers for led matrix delete
void sigint_handler(int signum){

	printf("closing... \n");	
	led_matrix_delete(matrix);
	exit(signum);

}

// multithread funcssdtions

void *datathread(void *args){
	printf("data thread active \n");

	while(1){
		sleep(100); // 100 seconds update
		int res = dataload();
		datasync();
		printf("auto data retrieve: %d \n", res);
	}
	return NULL;
}



int main(int argc, char **argv) {

	initPin(); // should come first	
	struct RGBLedMatrixOptions options;
	memset(&options, 0, sizeof(options));
	options.rows = 32;
	options.chain_length = 1;

	matrix=led_matrix_create_from_options(&options, &argc, &argv);

	if(matrix == NULL)
		return 1;
	
	realtime_canvas = led_matrix_get_canvas(matrix);

	// ctrl + c interrupt handler
	signal(SIGINT, sigint_handler);

	// create thread for data update
	pthread_t dataThread;
	pthread_create(&dataThread, NULL, datathread, NULL);

	//init variables
	userMatrix.currentViewIndex = -1;
	history.lastIndex = userMatrix.currentViewIndex;


	dataload();
	datasync();
	printData();

	//enter main loop
	loop();
}
void interruptTest(){
	printf("interrupt!!\n");
}
void leftShift(){
	// function for defaut led shift
	printf("int left \n ");	
//	led_canvas_clear(realtime_canvas);
	int size = 5;
	int j;
	int i;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int size_count = 0;
	//printf("yo");
	if(userMatrix.currentViewIndex > -1){ // view previous
		userMatrix.currentViewIndex --;
		history.lastIndex = userMatrix.currentViewIndex;
	}
	printf("screen stat: %d index \n", userMatrix.currentViewIndex);
	for(y = 0; y < 32; y++){
		size_count++;
		for(x = 0; x < 32; x++){
			random_r = rand() % 50;
			random_g = rand() % 50;
			random_b = rand() % 50;					
			led_canvas_set_pixel(realtime_canvas,y,x,x,0,2);
			if(size_count > size){
				led_canvas_set_pixel(realtime_canvas,y-size,x,0,0,0);
			}
			usleep(500);

		}
	}
	usleep(100);
	led_canvas_clear(realtime_canvas);

}

void rightShift(){
	// function for defaut led shift
	printf("int right \n");

//	led_canvas_clear(realtime_canvas);
	int size = 5;
	int i;
	int j;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int size_count=0;
	if(userMatrix.currentViewIndex < animation_n){
		userMatrix.currentViewIndex ++;
		history.lastIndex = userMatrix.currentViewIndex;
	}
	printf("screen stat: %d index \n", userMatrix.currentViewIndex);
	for(y = 31; y >= 0; y--){
		size_count++;
		for(x = 0; x < 32; x++){
			random_r = rand() % 50;
			random_g = rand() % 50;
			random_b = rand() % 50;					
			led_canvas_set_pixel(realtime_canvas,y,x,x,2,0);
			if(size_count > size){
				led_canvas_set_pixel(realtime_canvas,y+size,x,0,0,0);
			}
		
			usleep(500);
		}
	}
	usleep(100);
	led_canvas_clear(realtime_canvas);

}

void initPin(){

	wiringPiSetup();
	pinMode(LMOVEPIN, INPUT);
	pinMode(RMOVEPIN, INPUT);

	// attach interrupts
	
	wiringPiISR(LMOVEPIN, INT_EDGE_RISING, leftShift);
	wiringPiISR(RMOVEPIN, INT_EDGE_RISING, rightShift);	

}

int dataload() {
	FILE* fp = fopen("../client/PiryServer/clientUpload/data1.txt", "r");
	if (fp != NULL) {
		char dummy[BUFFER_SIZE], buffer[BUFFER_SIZE];
		char* tokptr;
		int tokindex = 0;

		int aniIndex = 0;
		int read_count; // why need? -> need by return value of fscanf func
		int hasMelody = 0;
		
		// first free before data
		free(animations);

		/* Read file info */
		fgets(dummy, sizeof(dummy), fp); // last update time
		//getchar();
		read_count = fscanf(fp, "%s %d", dummy, &animation_n);
		//getchar();
		animations = (Animation*)malloc(sizeof(Animation) * animation_n);
		printf("read fike info: a num: %d\n", animation_n);
		/* Read data */
		fseek(fp, 1, SEEK_CUR);
		while (!feof(fp)) {
			fgets(dummy, sizeof(dummy), fp); // read dummy : #animation index
			read_count = fscanf(fp, " %s %s", dummy, animations[aniIndex].name); // read name
			if(read_count == -1){
				break;
			}
			read_count = fscanf(fp, " %s %d", dummy, &animations[aniIndex].length); // read length
			animations[aniIndex].images = (Image*)malloc(sizeof(Image) * animations[aniIndex].length);
			fseek(fp, 1, SEEK_CUR);
			printf("readcount : %d, name: %s, length: %d\n", read_count, animations[aniIndex].name, animations[aniIndex].length);
			/* read delay */
			tokindex = 0;
			fgets(dummy, sizeof(dummy), fp);
			printf("dummy : %s \n", dummy);
			strcpy(buffer, dummy + 6);
			printf("buffer :%s \n", buffer); // buffer contain only delay numbers
			tokptr = strtok(buffer, " ");
			printf("first char of buffer!! : %c \n", buffer[0]);
			while (tokptr != NULL) {
				printf("token ptr : %s\n", tokptr);
				if(tokptr != NULL){
				animations[aniIndex].images[tokindex].delay = atoi(tokptr);
				tokindex++;
				}
				//animations[aniIndex].images[tokindex].delay = atoi(tokptr);
				printf("while read image delay: %d \n", animations[aniIndex].images[tokindex-1].delay);
				//tokindex++;
				tokptr = strtok(NULL, " ");
			}
			printf("read last image delay: %d \n", animations[aniIndex].images[tokindex-1].delay);
			read_count = fscanf(fp, " %s %d", dummy, &hasMelody); // read hasMelody
			printf("hasmelody : %d\n", hasMelody);
			if (hasMelody == 1) { // has song
				animations[aniIndex].melody.hasMelody = 1;

				/* read num_of_note*/
				read_count = fscanf(fp, " %s %d", dummy, &animations[aniIndex].melody.note_n);
				fseek(fp, 1, SEEK_CUR);

				/* read melody */
				tokindex = 0;
				fgets(dummy, sizeof(dummy), fp);
				strcpy(buffer, dummy + 10);
				animations[aniIndex].melody.frequency = (int*)malloc(sizeof(int) * animations[aniIndex].melody.note_n);
				tokptr = strtok(buffer, " ");
				while (tokptr != NULL) {
					animations[aniIndex].melody.frequency[tokindex++] = atoi(tokptr);
					tokptr = strtok(NULL, " ");
				}

				/* read duration */
				tokindex = 0;
				fgets(dummy, sizeof(dummy), fp);
				strcpy(buffer, dummy + 9);
				animations[aniIndex].melody.duration = (int*)malloc(sizeof(int) * animations[aniIndex].melody.note_n);
				tokptr = strtok(buffer, " ");
				while (tokptr != NULL) {
					animations[aniIndex].melody.duration[tokindex++] = atoi(tokptr);
					tokptr = strtok(NULL, " ");
				}
			}
			else { // don't have song
				printf("no melody \n");
				animations[aniIndex].melody.hasMelody = 0;
				animations[aniIndex].melody.note_n = 0;
				animations[aniIndex].melody.duration = NULL;
				animations[aniIndex].melody.frequency = NULL;
				fseek(fp, 1, SEEK_CUR);
			}

			/* read pixel info */
			fseek(fp, 1, SEEK_CUR);
			printf("animation length: %d frames \n", animations[aniIndex].length);
			for (int imageIndex = 0; imageIndex < animations[aniIndex].length; imageIndex++) {
				fgets(dummy, sizeof(dummy), fp); // dummy read : @image1
				//printf("image num : %s \n", dummy);
				for (int pixel_row = 0; pixel_row < IMAGE_SIZE; pixel_row++) {
					fgets(buffer, sizeof(buffer), fp);
					for (int pixel_col = 0; pixel_col < IMAGE_SIZE; pixel_col++) {
						animations[aniIndex].images[imageIndex].pixels[pixel_row][pixel_col] = buffer[pixel_col] - 48;
					}
				}
			}
			aniIndex++;
		}
		fclose(fp);
		animation_index = 0;
		melodyOn = 1;
		return 1;
	}
	else {
		printf("can't find data file.\n");
		return -1;
	}
}




void datasync(){
	// syncs old data.txt and updated one.
	if(history.anim_n != animation_n){
		history.isNewData = 1; // newData flag
		history.newIndex = history.lastIndex+1; // from next
		//history.lastIndex = animation_n;
		
		printf("new data recieved, diff : %d \n", animation_n - history.anim_n);
		history.anim_n = animation_n;
		//userMatrix.currentViewIndex = history.lastIndex; // uncomment to see latest in every update
	}
	else{
		printf("data is same \n");
	}



}


void printData() {
	printf("show animation\n\n");
	for (int i = 0; i < animation_n; i++) {
		printf("#%d animation, name : %s, length : %d \n", i, animations[i].name, animations[i].length);
		printf("hasMelody : %d\n", animations[i].melody.hasMelody);
		printf("num_of_note : %d\n", animations[i].melody.note_n);
		printf("melody : ");
		for (int iter = 0; iter < animations[i].melody.note_n; iter++) {
			printf("%d ", animations[i].melody.frequency[iter]);
		}
		printf("\nduration : ");
		for (int iter = 0; iter < animations[i].melody.note_n; iter++) {
			printf("%d ", animations[i].melody.duration[iter]);
		}
		printf("\n");
		for (int j = 0; j < animations[i].length; j++) {
			printf("animation %d-%d delay:% d\n", i, j, animations[i].images[j].delay);

			for (int k = 0; k < IMAGE_SIZE; k++) {
				for (int l = 0; l < IMAGE_SIZE; l++) {
					printf("%d", animations[i].images[j].pixels[k][l]);
				}
				printf("\n");
			}
			printf("\n");
			
			print_matrix(animations[i].images[j].pixels, realtime_canvas, animations[i].images[j].delay);
		}
	}

}


void loop(){
	
	while(1){
		//led_canvas_clear(realtime_canvas);
		int lpinin, rpinin;
		
		if(userMatrix.currentViewIndex == -1){ // start data
			printLEDDefault();
		}
		else if(userMatrix.currentViewIndex >= animation_n){ // end of data
			printLEDEnd();
		}
		else{
			printf("same!!\n");
			printLedData();
		}
		//detect pin
		
		lpinin = digitalRead(LMOVEPIN);
		rpinin = digitalRead(RMOVEPIN);
		
	//	printf("lpin %d, rpin: %d \n", lpinin, rpinin);
		if(rpinin == 1){
	//		printf("lpin %d, rpin: %d \n", lpinin, rpinin);
	//		rightShift();
		}
		if(lpinin == 1){
	//		printf("lpin %d, rpin: %d \n", lpinin, rpinin);
	//		leftShift();
		}

	}

}


void printLEDDefault() {
	
	int speed = 2000;
	int rpinin=0, lpinin=0;		
	int i;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int shift_flag = 0;
	for(y = 0; y < 32; y++){
		shift_flag = 0;
		for(x = 0; x < 32; x++){
			random_r = rand() % 50;
			random_g = rand() % 50;
			random_b = rand() % 60;					
			led_canvas_set_pixel(realtime_canvas,y,x,x,y,random_b);
			usleep(speed);
			lpinin = digitalRead(LMOVEPIN);
			rpinin = digitalRead(RMOVEPIN);
			if(lpinin == 1 || rpinin == 1) {
				shift_flag = 1;
				break;
			}
		}
		if(shift_flag == 1){
			printf("flag! \n");
			break;
		}
	}
	if(shift_flag == 0){
		sleep(1);
		led_canvas_clear(realtime_canvas);
	}

}

void printLEDEnd(){
	int speed = 2000;
	int rpinin=0, lpinin=0;		
	int i;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int shift_flag = 0;
	for(y = 31; y >= 0; y--){
		shift_flag = 0;
		for(x = 0; x < 32; x++){
			random_r = rand() % 50;
			random_g = rand() % 50;
			random_b = rand() % 60;					
			led_canvas_set_pixel(realtime_canvas,y,x,y,random_b,x);
			usleep(speed);
			lpinin = digitalRead(LMOVEPIN);
			rpinin = digitalRead(RMOVEPIN);
			if(lpinin == 1 || rpinin == 1) {
				shift_flag = 1;
				break;
			}
		}
		if(shift_flag == 1){
			printf("flag! \n");
			break;
		}
	}
	if(shift_flag == 0){
		sleep(1);
		led_canvas_clear(realtime_canvas);
	}



}

void printLedData(){
	// print reserved data according to cursor
	int rpinin=0, lpinin=0;		
	int shift_flag = 0;

	
	for (int j = 0; j < animations[userMatrix.currentViewIndex].length; j++) {
		//printf("print led data!!\n");
		if(userMatrix.currentViewIndex <  animation_n){
		
			print_matrix(animations[userMatrix.currentViewIndex].images[j].pixels, realtime_canvas, animations[userMatrix.currentViewIndex].images[j].delay);
		}
		//print_matrix(animations[userMatrix.currentViewIndex].images[j].pixels, realtime_canvas, animations[userMatrix.currentViewIndex].images[j].delay);
	}

}


void printMelody() {
	int buzzerPin; // 임시로
	if (melodyOn) {
		for (int noteIndex = 0; noteIndex < animations[animation_index].melody.note_n; noteIndex++) {
			if (animations[animation_index].melody.frequency[noteIndex] != -1) { // -1일 경우 rest
				//Tone(buzzerPin, animations[animation_index].melody.frequency[noteIndex]);
			}
			//delay(1000 / 8 * animations[animation_index].melody.duration[noteIndex]);
			//noTone(buzzerPin);
		}
	}
}

void shiftImage() {
	for (int ledIndex = 0; ledIndex < IMAGE_SIZE; ledIndex++) {
		for (int i = 0; i < IMAGE_SIZE - 1; i++) {
			for (int j = 0; j < IMAGE_SIZE; j++) {
			//	digitalWrite(i * 100 + j, animations[animation_index].images->pixels[j][i + 1]);
			
			}
		}
		for (int j = 0; j < IMAGE_SIZE; j++) {
			//digitalWrite(31 * 100 + j, 0);
		}
		//delay(100);
	}
	//delay(1000);
}


void print_matrix(char image[32][32], struct LedCanvas *canvas, int speed){
	int i;
	int x, y;
	int r = 100;
	for(y = 0; y < 32; y++){
		for(x = 0; x < 32; x++){
			if(image[x][y] == 1){
				led_canvas_set_pixel(canvas,y,x,r,0,0);
			}
		
		}
	}
	//printf("sleeping !! \n");
	usleep(speed*100);
	led_canvas_clear(canvas);
}
