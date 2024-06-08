/*
 * Universidade de Caxias do Sul
 * Authors: Anderson Pastore Rizzi and Lucas Klug Arndt.
 */



/*******************************************************************************/
/* INCLUSION OF LIBRARIES AND PREPROCESSING SETTINGS                           */
/*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#pragma pack(1)



/*******************************************************************************/
/* DEFINITION OF TYPES AND STRUCTURES                                          */
/*******************************************************************************/

struct headerBPM {
    unsigned short tipo;
	unsigned int tamanho_arquivo;
	unsigned short reservado1;
	unsigned short reservado2;
	unsigned int offset;
	unsigned int tamanho_image_header;
	int largura;
	int altura;
	unsigned short planos;
	unsigned short bits_por_pixel;
	unsigned int compressao;
	unsigned int tamanho_imagem;
	int largura_resolucao;
	int altura_resolucao;
	unsigned int numero_cores;
	unsigned int cores_importantes;
};

struct pixel {
    unsigned char r, g, b;
};

typedef struct headerBPM HeaderBPM;
typedef struct pixel Pixel;



/*******************************************************************************/
/* FUNCTIONS HEADER                                                            */
/*******************************************************************************/

void grayscaleFilter(int imageWidth, int imageHeight, int numberOfProcesses, int processID, Pixel *pixels);
void medianFilter(int imageWidth, int imageHeight, int numberOfProcesses, int processID, int maskSize, Pixel *pixels);
void prewittFilter(int imageWidth, int imageHeight, int numberOfProcesses, int processID, Pixel *pixels, Pixel *newPixels);



/*******************************************************************************/
/* MAIN FUNCTION                                                               */
/*******************************************************************************/

int main(int argc, char **argv) {
    // Declaration of local variables.
	int i, j, readyCounter;
    Pixel pixelTemp;



    // Checking input parameters.
	if (argc != 3){
		printf("%s <median_filter_mask_size> <number_of_processes>\n", argv[0]);
		
	}

	int medianFilterMaskSize = atoi(argv[1]);
	int numberOfProcesses = atoi(argv[2]);

    if (medianFilterMaskSize % 2 == 0) {
        printf("Error: The median filter mask size must be odd.\n");
        exit(0);
    }



    // Opening the input file.
    FILE *in = fopen("borboleta.bmp", "rb");

    if (in == NULL) {
        printf("Error opening input file.\n");
        exit(0);
    }

    

    // Leitura do cabeçalho do arquivo BPM.
    HeaderBPM c;
    fread(&c, sizeof(HeaderBPM), 1, in);



    // Shared memory allocation.
    int tam_img = c.altura * c.largura;
	int shmid1 = shmget(0x19, c.altura * c.largura * sizeof(Pixel *), IPC_CREAT | 0600);
    Pixel *pixels = shmat(shmid1, 0, 0);

    int shmid2 = shmget(0x21, c.altura * c.largura * sizeof(Pixel *), IPC_CREAT | 0600);
    Pixel *newPixels = shmat(shmid2, 0, 0);

    int shmid3 = shmget(0x23, numberOfProcesses * sizeof(int), IPC_CREAT | 0600);
    int *readyVector = shmat(shmid3, 0, 0);

    for (i = 0; i < numberOfProcesses; i++) {
        readyVector[i] = 0;    
    }

    if (pixels == NULL || newPixels == NULL) {
        printf("Error in memory allocation.\n");
        fclose(in);
        exit(0);
    }
    


    // Reading and storing image pixels in shared memory.
    for (i = 0; i < c.altura; i++) {
        for (j = 0; j < c.largura; j++) {
            fread(&pixelTemp, sizeof(Pixel), 1, in);
            pixels[i * c.largura + j] = pixelTemp;
        }
    }



    // Creation of processes.
	int processID = 0;
	for(i = 1; i < numberOfProcesses; i++){
		int pid = fork();
		if (pid == 0){
			processID = i;
			break;
		}
	}
    


    // Application of filters (Warning: All filters must be applied).
    grayscaleFilter(c.largura, c.altura, numberOfProcesses, processID, pixels);
    readyVector[processID] += 1;
    readyCounter = 0;
    i = 0;

    while (1) {
        if (readyVector[i++] == 1) readyCounter++;
        if (readyCounter >= numberOfProcesses) break;
        if (i == numberOfProcesses) {
            i = 0;
            readyCounter = 0;
        }
    }


    medianFilter(c.largura, c.altura, numberOfProcesses, processID, medianFilterMaskSize, pixels);
    readyVector[processID] += 1;
    readyCounter = 0;
    i = 0;

    while (1) {
        if (readyVector[i++] == 2) readyCounter++;
        if (readyCounter >= numberOfProcesses) break;
        if (i == numberOfProcesses) {
            i = 0;
            readyCounter = 0;
        }
    }


    prewittFilter(c.largura, c.altura, numberOfProcesses, processID, pixels, newPixels);
    readyVector[processID] += 1;
    readyCounter = 0;
    i = 0;

    while (1) {
        if (readyVector[i++] == 3) readyCounter++;
        if (readyCounter >= numberOfProcesses) break;
        if (i == numberOfProcesses) {
            i = 0;
            readyCounter = 0;
        }
    }


    // Saving the image and closing the program.
    if (processID == 0) {
        // The parent process waits for all child processes to finish executing.
		for (i = 1; i < numberOfProcesses; i++) {
			wait(NULL);  
		}

        // Opening the output file.
        FILE *out = fopen("output.bmp", "wb");
        
        if (out == NULL) {
            printf("Error generating the output file.\n");
            fclose(in);
            exit(0);
        }

        fwrite(&c, sizeof(HeaderBPM), 1, out);

        for (i = 0; i < c.altura; i++) {
            for (j = 0; j < c.largura; j++) {
                pixelTemp = newPixels[i * c.largura + j];
                fwrite(&pixelTemp, sizeof(Pixel), 1, out);
            }
        }
        
		shmdt(pixels);
		shmctl(shmid1, IPC_RMID, 0);
        shmdt(newPixels);
        shmctl(shmid2, IPC_RMID, 0);
        shmdt(readyVector);
        shmctl(shmid3, IPC_RMID, 0);
        fclose(in);
        fclose(out);
	}

    return 0;
}



/*******************************************************************************/
/* AUXILIARY FUNCTIONS                                                         */
/*******************************************************************************/

// This function returns the median of a vector.
int median(int vet[], int n) {
    int i, j;

    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (vet[i] > vet[j]) {
                int temp = vet[i];
                vet[i] = vet[j];
                vet[j] = temp;
            }
        }
    }

    return vet[n / 2];
}



// This function converts a BMP image to grayscale.
void grayscaleFilter(int imageWidth, int imageHeight, int numberOfProcesses, int processID, Pixel *pixels) {
    int i, j, position;

    int numberOfRows = imageHeight / numberOfProcesses;
    int initialLine = processID * numberOfRows;
    int lastLine = (initialLine + numberOfRows) - 1;

    if (processID == (numberOfProcesses - 1)) lastLine = imageHeight - 1;

    for (i = initialLine; i <= lastLine; i++) {
        for (j = 0; j < imageWidth; j++) {
            position = (i * imageWidth) + j;
            Pixel currentPixel = pixels[position];
            unsigned char gray = 0.2126 * currentPixel.r + 0.7152 * currentPixel.g + 0.0722 * currentPixel.b;
            currentPixel.r = currentPixel.g = currentPixel.b = gray;
            pixels[position] = currentPixel;
        }
    }
}



// This function applies a median filter to a BMP image with a variable size mask.
void medianFilter(int imageWidth, int imageHeight, int numberOfProcesses,  int processID, int maskSize, Pixel *pixels) {
    int i, j;

    int numberOfRows = imageHeight / numberOfProcesses;
    int initialLine = processID * numberOfRows;
    int lastLine = (initialLine + numberOfRows) - 1;
    if (processID == (numberOfProcesses - 1)) lastLine = imageHeight - 1;

    if (maskSize % 2 == 0) maskSize++;
    int maskVectorSize = maskSize * maskSize;
    int *vet_r = (int *) malloc(maskVectorSize * sizeof(int));
    int *vet_g = (int *) malloc(maskVectorSize * sizeof(int));
    int *vet_b = (int *) malloc(maskVectorSize * sizeof(int));

    int halfMaskWidth = maskSize / 2;

    for (i = initialLine; i <= lastLine; i++) {
        for (j = 0; j < imageWidth; j++) {
            // Calculates the start and end positions of the mask.
            int x1 = j - halfMaskWidth;
            int x2 = j + halfMaskWidth;
            int y1 = i - halfMaskWidth;
            int y2 = i + halfMaskWidth;

            if (x1 < 0) x1 = 0;
            if (x2 > imageWidth - 1) x2 = imageWidth - 1;
            if (y1 < 0) y1 = 0;
            if (y2 > imageHeight - 1) y2 = imageHeight - 1;

            int aux = x1;
            int counter = 0;

            // Selects pixels according to the mask.
            for (; y1 <= y2 ; y1++) {
                for (x1 = aux; x1 <= x2; x1++) {
                    vet_r[counter] = pixels[y1 * imageWidth + x1].r;
                    vet_g[counter] = pixels[y1 * imageWidth + x1].g;
                    vet_b[counter] = pixels[y1 * imageWidth + x1].b;
                    counter++;
                }
            }

            // Calculates the median for each vector.
            pixels[i * imageWidth + j].r = median(vet_r, counter);
            pixels[i * imageWidth + j].g = median(vet_g, counter);
            pixels[i * imageWidth + j].b = median(vet_b, counter);
        }
    }

    free(vet_r);
    free(vet_g);
    free(vet_b);
}



// This function applies a Prewitt filter to a BMP image.
void prewittFilter(int imageWidth, int imageHeight, int numberOfProcesses, int processID, Pixel *pixels, Pixel *newPixels) {
    int i, j, k;

    int masc_gx[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
	int masc_gy[9] = {-1, -1, -1, 0,  0,  0, 1,  1,  1};
    
    int numberOfRows = imageHeight / numberOfProcesses;
    int initialLine = processID * numberOfRows;
    int lastLine = (initialLine + numberOfRows) - 1;
    if (processID == (numberOfProcesses - 1)) lastLine = imageHeight - 1;

    unsigned char *vet_r = (unsigned char *) malloc(3 * 3 * sizeof(unsigned char));

    int halfMaskWidth = 1;

    for (i = initialLine; i <= lastLine; i++) {
        for (j = 0; j < imageWidth; j++) {

            // Calculates the start and end positions of the mask.
            int x1 = j - halfMaskWidth;
            int x2 = j + halfMaskWidth;
            int y1 = i - halfMaskWidth;
            int y2 = i + halfMaskWidth;

            if (x1 < 0) x1 = 0;
            if (x2 > imageWidth - 1) x2 = imageWidth - 1;
            if (y1 < 0) y1 = 0;
            if (y2 > imageHeight - 1) y2 = imageHeight - 1;

            int aux = x1;
            int counter = 0;

            int gx = 0;
            int gy = 0;

            // Selects mask pixels.
            for (; y1 <= y2 ; y1++) {
                for (x1 = aux; x1 <= x2; x1++) {
                    // Only one color channel is used, as they all have the same value, as the image is in grayscale.
                    vet_r[counter++] = pixels[y1 * imageWidth + x1].r;
                }
            }

            // For pixels that are not part of the image border, calculate the gradient.
            if (counter == 9) {
                for (k = 0; k < 9; k++) {
                    gy += vet_r[k] * masc_gy[k];
                    gx += vet_r[k] * masc_gx[k];
                }

                int grad = (int) sqrt((gx * gx) + (gy * gy));
                grad = grad < 0 ? 0 : ((grad > 255) ? 255 : grad);

                newPixels[i * imageWidth + j].r = grad;
                newPixels[i * imageWidth + j].g = grad;
                newPixels[i * imageWidth + j].b = grad;
            } else {
                newPixels[i * imageWidth + j] = pixels[i * imageWidth + j];
            }
        }
    }

    free(vet_r);
}
