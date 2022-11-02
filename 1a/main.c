#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>

void processStreamTabs(FILE *inputStream, int tabSpaces, FILE *outputStream)
{
    char *line = NULL;
    size_t size = 0;

    /* read lines in input stream */
    while (getline(&line, &size, inputStream) != EOF)
    {
        int lineIndex = 0;
        int outputPosition = 0;

        /* loop through characters in line */
        char character = NULL; 
        while((character = line[lineIndex++]) != '\0')
        {

            /* if the current character is a tabstop */
            if(character == '\t')
            {
                int newPosition = tabSpaces * ((outputPosition / tabSpaces) + 1);

                /* insert spaces */
                while(outputPosition < newPosition) 
                {
                    fprintf(outputStream, " ");
                    outputPosition++;
                }
            }

            /* else just print the current character */
            else {
                fprintf(outputStream, "%c", character);
                outputPosition++;
            }
        }
    }
}

int main(int argc, char *argv[])
{

    /* parsed options */
    char* outFile = NULL;
    int tabDistance = 8;

    /* get options */
    opterr = 0;
    int opt;
    while ((opt = getopt(argc, argv, "t:o:")) != -1)
    {
        switch (opt)
        {
            case 't':
                tabDistance = strtol(optarg, NULL, 10);
                break;
            case 'o':
                outFile = optarg;
                break;
            default:
                printf("unsupported option, exiting\n");
                exit(EXIT_FAILURE);
        }
    }

    /* try to open out file */
    FILE* outputStream = stdout;
    if(outFile != NULL){
        outputStream = fopen(outFile, "w");
        
        /* if opened stream is null,  fall back to stdout */
        if (outputStream == NULL)
        {
            printf(" # output file errored. writing to stdout instead");
            outputStream = stdout;
        }
    }
    
    /* print parsed options */
    printf(" - tab distance is %i spaces\n", tabDistance);
    if (outputStream != stdout)  printf(" - out file is %s \n", outFile);
    else printf(" - printing output to console\n");

    /* check if there are positional arguments left, else read input from stdin */
    if(optind < argc){

        /*  remaining arguments -> filenames; perform action on each file */
        for (; optind < argc; optind++) {

            /* open file */
            char* file = argv[optind];
            printf(" - processing file %s..\n", file);
            FILE *fileStream = fopen(file, "r");

            /* check if file succeeded */
            if(fileStream != NULL){
                processStreamTabs(fileStream, tabDistance, outputStream);
                fclose(fileStream);
                printf("\n - finished file processing\n");
            }
            else printf("\n - couldn't open file, skipping\n");
        }
    }
    else {

        /* no positional arguments -> read from stdin */
        printf(" - no input file(s) specified, enter text:\n");
        processStreamTabs(stdin, tabDistance, outputStream);
        printf("\n - finished inut processing\n");    
    }

    /* close output stream */
    if(outputStream != stdout) fclose(outputStream);
}