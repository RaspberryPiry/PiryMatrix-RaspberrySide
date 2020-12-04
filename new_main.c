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
#include <wiringSerial.h>
#include <errno.h>


#include "led-matrix-c.h" // for matrix controls

//hardware pins
#define DMOVEPIN 25
#define UMOVEPIN 27
#define LMOVEPIN 28
#define RMOVEPIN 29


typedef struct {
	int delay;
	int pixels[3][IMAGE_SIZE][IMAGE_SIZE];
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
	int weatherViewFlag;
}MatrixStatus;


/*global variable*/
Animation* animations;
Animation* boot;
Animation* weather;
MatrixStatus userMatrix;
History history;

int animation_n;
int animation_index;
int melodyOn;
int shift_flag=0;


//int boot_animation_n;
//int weather_animation_n;


struct RGBLedMatrix *matrix;
struct LedCanvas *realtime_canvas;


/*method*/
int dataload();
int loadBoot(char *dir);
void printData();
void printLED();
void printMelody();
//void init_matrix();
void print_matrix(int image[3][32][32], struct LedCanvas *canvas, int speed);
void printLEDDefault();
void loop();
void leftShift();
void rightShift();
void upShift();
void downShift();
void initPin();
void datasync();
void printLedData();
void printLEDEnd();
void printLedWeather();
int *sendSerial();
void itoa(int n, char s[]);
void reverse(char s[]);


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
void leftShift(){
	// function for defaut led shift
	printf("int left \n ");
	shift_flag = 1;	
//	led_canvas_clear(realtime_canvas);
	int size = 5;
	int j;
	int i;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int size_count = 0;

	pthread_t serialThread;
	if(userMatrix.currentViewIndex > -1){ // view previous
		userMatrix.currentViewIndex --;
		history.lastIndex = userMatrix.currentViewIndex;
		pthread_create(&serialThread, NULL, sendSerial, NULL);
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
	shift_flag = 0;
	led_canvas_clear(realtime_canvas);

}

void rightShift(){
	// function for defaut led shift
	printf("int right \n");
	shift_flag = 1;
	int size = 5;
	int i;
	int j;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int size_count=0;
	pthread_t serialThread;
	if(userMatrix.currentViewIndex < animation_n){
		userMatrix.currentViewIndex ++;
		history.lastIndex = userMatrix.currentViewIndex;
	//	pthread_create(&serialThread, NULL, sendSerial, NULL);
		printf("view index + 1 \n");
	}
	if(userMatrix.currentViewIndex < animation_n - 1){
		pthread_create(&serialThread, NULL, sendSerial, NULL);
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
	shift_flag = 0;
	led_canvas_clear(realtime_canvas);

}

void upShift(){
	// up to down
	int size = 5;
	int i;
	int j;
	int x, y;
	int r = 100;
	int random_r, random_g, random_b;
	int size_count=0;
	
	shift_flag = 1;
	
	// set weather view flag
	userMatrix.weatherViewFlag = 1;
	
	printf("screen stat: %d index, weather: %d \n", userMatrix.currentViewIndex, userMatrix.weatherViewFlag);
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
	printf("shift up!! \n");
	led_canvas_clear(realtime_canvas);
	shift_flag = 0;
}

void downShift(){
	// down to up

	printf("down shift! \n");
}




void initPin(){

	wiringPiSetup();
	pinMode(LMOVEPIN, INPUT);
	pinMode(RMOVEPIN, INPUT);
	pinMode(UMOVEPIN, INPUT);
	pinMode(DMOVEPIN, INPUT);

	// attach interrupts
	
	wiringPiISR(LMOVEPIN, INT_EDGE_RISING, leftShift);
	wiringPiISR(RMOVEPIN, INT_EDGE_RISING, rightShift);	
	wiringPiISR(UMOVEPIN, INT_EDGE_RISING, upShift);	
	wiringPiISR(DMOVEPIN, INT_EDGE_RISING, downShift);	

}

int loadImages(Animation *save, char *dir){

	FILE* fp = fopen(dir, "r");
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
		printf("read file info: a num: %d\n", animation_n);
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
				printf("image num : %s \n", dummy);
				for (int pixel_row = 0; pixel_row < IMAGE_SIZE; pixel_row++) {
					fgets(dummy, sizeof(dummy), fp);
					printf("pixel row : %s \n", dummy);

              				tokptr = strtok(dummy, " ");
 			                for (int pixel_col = 0, colorIndex = 0; pixel_col < IMAGE_SIZE; pixel_col++) {
                  				colorIndex = 0;
       			          	 	buffer[0] = '0';
                  				buffer[1] = 'x';
				                strncpy(buffer + 2, tokptr, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex++][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				strncpy(buffer + 2, tokptr + 2, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex++][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				strncpy(buffer + 2, tokptr + 4, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				tokptr = strtok(NULL, " ");
               				}
               				printf("\n");
            			}
         		}
         		printf("finish");
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

int dataload() {
	FILE* fp = fopen("rgbdata.txt", "r");
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
		printf("read file info: a num: %d\n", animation_n);
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
				printf("image num : %s \n", dummy);
				for (int pixel_row = 0; pixel_row < IMAGE_SIZE; pixel_row++) {
					fgets(dummy, sizeof(dummy), fp);
					printf("pixel row : %s \n", dummy);

              				tokptr = strtok(dummy, " ");
 			                for (int pixel_col = 0, colorIndex = 0; pixel_col < IMAGE_SIZE; pixel_col++) {
                  				colorIndex = 0;
       			          	 	buffer[0] = '0';
                  				buffer[1] = 'x';
				                strncpy(buffer + 2, tokptr, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex++][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				strncpy(buffer + 2, tokptr + 2, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex++][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				strncpy(buffer + 2, tokptr + 4, 2);
                  				buffer[4] = '\0';
                  				animations[aniIndex].images[imageIndex].pixels[colorIndex][pixel_row][pixel_col] = strtol(buffer, NULL, 16);
                  				tokptr = strtok(NULL, " ");
               				}
               				printf("\n");
            			}
         		}
         		printf("finish");
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
	}
	
	/*
	// show boot (temporory) //
	for (int j = 0; j < animations[3].length; j++) {

		for (int k = 0; k < IMAGE_SIZE; k++) {
			for (int l = 0; l < IMAGE_SIZE; l++) {
			}
			printf("\n");
		}
		printf("\n");
			
		print_matrix(animations[3].images[j].pixels, realtime_canvas, animations[3].images[j].delay);
	}
	*/

}


void loop(){
	
	while(1){
		int lpinin, rpinin;
		
		if(userMatrix.currentViewIndex == -1){ // start data
			printLEDDefault();
		}
		else if(userMatrix.currentViewIndex >= animation_n){ // end of data
			printLEDEnd();
		}
		else{
			printLedData();
		}

		if(userMatrix.weatherViewFlag == 1){
			printLedWeather();
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


int *sendSerial(){
	
	char *freqs;
	char *duras;
	char *title;
	char buffer[10];
	char space[] = "/";
	char newline[] = "&";
	char r_newline[] = "\n";
	char dd;	
	int fd;
	int currentIndex = userMatrix.currentViewIndex;
	char len[10];
	char data[1000];

	freqs = (char*)malloc(animations[currentIndex].melody.note_n * sizeof(int)+animations[currentIndex].melody.note_n * sizeof(char));
	duras = (char*)malloc(animations[currentIndex].melody.note_n * sizeof(int)+animations[currentIndex].melody.note_n * sizeof(char));

	
	itoa(animations[currentIndex].melody.note_n, len);
	strcat(data, len);


	for (int iter = 0; iter < animations[currentIndex].melody.note_n; iter++) {
		for (int i = 0; i < 10; i++) {
        		buffer[i] = '\0';
     		}
		itoa(animations[currentIndex].melody.frequency[iter], buffer);
		printf("buffer: %s \n", buffer);
		strcat(freqs, buffer);
		if(iter < animations[currentIndex].melody.note_n - 1){
			strcat(freqs, space);
		}

	}
	strcat(data,newline);
	strcat(data,freqs);
	printf("\n");
	for (int iter = 0; iter < animations[currentIndex].melody.note_n; iter++) {
		for (int i = 0; i < 10; i++) {
        		buffer[i] = '\0';
     		}
		itoa(animations[currentIndex].melody.duration[iter], buffer);
		printf("dur buffer: %s \n", buffer);
		strcat(duras, buffer);
		if(iter < animations[currentIndex].melody.note_n - 1){
			strcat(duras, space);
		}
	}
	printf("\n");
	strcat(data, newline);
	strcat(data, duras);
	strcat(data, newline);
	strcat(data, animations[currentIndex].name);
	strcat(data, r_newline);


	
	if ((fd = serialOpen("/dev/ttyACM0", 9600)) < 0)
   	{		
      		fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
      		return NULL;
   	}else{
		printf("Serial Open!! \n");
	}
//	serialPuts(fd, len);
//   	serialPuts(fd, freqs);
//	serialPuts(fd, duras);
	serialPuts(fd, data);

	printf("-- printing arduino serial -- \n");
	sleep(1); // wait for arduino return
	while (serialDataAvail (fd))
    	{
      		printf ("%c", serialGetchar (fd)) ;
      		//fflush (stdout) ;
    	}
	//printf("serial_recieved_data : %s \n", data);
//	printf("serial_recieved_data : %c \n", dd);
	//	

	return NULL;

}

void itoa(int n, char s[])
{
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
}  
void reverse(char s[])
{
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
} 

void printLEDDefault() {
	
	int speed = 2000;
	int rpinin=0, lpinin=0;		
	int i;
	int x, y;
	int random_r, random_g, random_b;
	
	char dd;	
	int fd;
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
	int random_r, random_g, random_b;
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

void printLedWeather(){
	int random_r, random_g, random_b;
	int speed = 2000;
	userMatrix.weatherViewFlag = 1;
	
	for(int y = 31; y >= 0; y--){
		shift_flag = 0;
		for(int x = 0; x < 32; x++){
			random_r = rand() % 50;
			random_g = rand() % 50;
			random_b = rand() % 60;					
			led_canvas_set_pixel(realtime_canvas,y,x,y,random_b,x);
			usleep(speed);
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

	userMatrix.weatherViewFlag = 0;
}

void printLedData(){
	// print reserved data according to cursor
	int rpinin=0, lpinin=0;		
	char data[500];
	char dd;
	
	for (int j = 0; j < animations[userMatrix.currentViewIndex].length; j++) {
		if(userMatrix.currentViewIndex <  animation_n){
			
			if(shift_flag == 1){
				printf("breaking\n");
				break;
			}
			if(userMatrix.weatherViewFlag == 1){
				printf("weather view showing, break \n");
				break;
			}

			print_matrix(animations[userMatrix.currentViewIndex].images[j].pixels, realtime_canvas, animations[userMatrix.currentViewIndex].images[j].delay);
		}
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


void print_matrix(int image[3][32][32], struct LedCanvas *canvas, int speed){
	int i;
	int x, y;
	int r = 100;
	for(y = 0; y < 32; y++){
		for(x = 0; x < 32; x++){		
			//printf("{%d, %d, %d} ", image[0][x][y], image[1][x][y], image[2][x][y]);
			led_canvas_set_pixel(canvas,x,y,image[0][x][y],image[1][x][y],image[2][x][y]);
		
		}
	}
	//printf("sleeping !! \n");
	usleep(speed*1000);
	led_canvas_clear(canvas);
}



