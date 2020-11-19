#define _CRT_SECURE_NO_WARNINGS
#define TEXT_MAX_SIZE 10 // using textlcd 
#define IMAGE_SIZE 32 // 32*32 pixel
#define BUFFER_SIZE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led-matrix-c.h" // for matrix controls


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

/*global variable*/
Animation* animations;
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

int main(int argc, char **argv) {
	
	struct RGBLedMatrixOptions options;
	memset(&options, 0, sizeof(options));
	options.rows = 32;
	options.chain_length = 1;

	matrix=led_matrix_create_from_options(&options, &argc, &argv);

	if(matrix == NULL)
		return 1;
	
	realtime_canvas = led_matrix_get_canvas(matrix);
//	led_canvas_get_size(realtime_canvas, &width, &height);
	

	dataload();
	printData();
}

int dataload() {
	FILE* fp = fopen("data.txt", "r");
	if (fp != NULL) {
		char dummy[BUFFER_SIZE], buffer[BUFFER_SIZE];
		char* tokptr;
		int tokindex = 0;

		int aniIndex = 0;
		int read_count; // why need? -> need by return value of fscanf func
		int hasMelody = 0;

		/* Read file info */
		fgets(dummy, sizeof(dummy), fp); // last update time
		read_count = fscanf(fp, "%s %d", dummy, &animation_n);
		animations = (Animation*)malloc(sizeof(Animation) * animation_n);

		/* Read data */
		fseek(fp, 2, SEEK_CUR);
		while (!feof(fp)) {
			fgets(dummy, sizeof(dummy), fp); // read dummy : #animation index
			read_count = fscanf(fp, " %s %s", dummy, animations[aniIndex].name); // read name
			read_count = fscanf(fp, " %s %d", dummy, &animations[aniIndex].length); // read length
			animations[aniIndex].images = (Image*)malloc(sizeof(Image) * animations[aniIndex].length);
			fseek(fp, 2, SEEK_CUR);

			/* read delay */
			tokindex = 0;
			fgets(dummy, sizeof(dummy), fp);
			strcpy(buffer, dummy + 6);
			tokptr = strtok(buffer, " ");
			while (tokptr != NULL) {
				animations[aniIndex].images[tokindex].delay = atoi(tokptr);
				tokindex++;
				tokptr = strtok(NULL, " ");
			}

			read_count = fscanf(fp, " %s %d", dummy, &hasMelody); // read hasMelody

			if (hasMelody == 1) { // has song
				animations[aniIndex].melody.hasMelody = 1;

				/* read num_of_note*/
				read_count = fscanf(fp, " %s %d", dummy, &animations[aniIndex].melody.note_n);
				fseek(fp, 2, SEEK_CUR);

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
				animations[aniIndex].melody.hasMelody = 0;
				animations[aniIndex].melody.note_n = 0;
				animations[aniIndex].melody.duration = NULL;
				animations[aniIndex].melody.frequency = NULL;
				fseek(fp, 2, SEEK_CUR);
			}

			/* read pixel info */
			fseek(fp, 2, SEEK_CUR);
			for (int imageIndex = 0; imageIndex < animations[aniIndex].length; imageIndex++) {
				fgets(dummy, sizeof(dummy), fp); // dummy read : @image1
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
			
			print_matrix(animations[i].images[j].pixels, realtime_canvas, animations[i].images[j].delay+5000);
		}
	}

	printLEDDefault();

}

void printLEDDefault() {
	
	int speed = 2000;
	while (1) {
		
		int i;
		int x, y;
		int r = 100;
		int random_r, random_g, random_b;
		for(y = 0; y < 32; y++){
			for(x = 0; x < 32; x++){
				random_r = rand() % 50;
				random_g = rand() % 50;
				random_b = rand() % 50;					
				led_canvas_set_pixel(realtime_canvas,y,x,x,y,random_b);
				usleep(speed);
			}
		}
	sleep(1);
	led_canvas_clear(realtime_canvas);
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
	for(i = 0; i < speed; i++){
		for(y = 0; y < 32; y++){
			for(x = 0; x < 32; x++){
				if(image[x][y] == 1){
					led_canvas_set_pixel(canvas,y,x,r,0,0);
				}
			}
		}
	}
	led_canvas_clear(canvas);
}
