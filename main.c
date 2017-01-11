/* main.c Contains main function handling API initialization and thread creation
 * Copyright (c) 2017 Jay Biernat
 * Copyright (c) 2017 University of Rochester
 *
 * This file is part of Musical Mood Detector and Visualizer
 *
 * Musical Mood Detector and Visualizer is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Musical Mood Detector and Visualizer is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Musical Mood Detector and Visualizer.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>

#include <fftw3.h>
#include <portaudio.h>
#include "featureExtraction.h"

#include "moodRecognition.h"

#include <SDL.h>
#include "imageDisplay.h"

int getuint( void );    /* Input retrieval and validation */
void printInfo( void ); /* Prints license and explanation of program */

int main( int argc, char* argv[] )
{
    fe_extraction_info          extraction_info;
    fe_extraction_thread_data   portAudioData;
    portAudioData.init_success  = 0;

    PaStream            *stream;
    PaError             err;
    PaStreamParameters  inputParameters;
    PaStreamParameters  outputParameters;

    const PaDeviceInfo  *deviceInfo;
    int                 numDevices;
    int                 chosenDeviceNum;
    int                 numInputDevices = 0;
    int                 numOutputDevices = 0;
    int                 *input_list_num = NULL;
    int                 *output_list_num = NULL;

    mr_detection_thread_data        moodDetectionData;
    moodDetectionData.init_success  = 0;

    id_imageDisplay_data        displayData;
    displayData.init_success    = 0;

    id_textureThreadStruct              textureUpdateData;
    textureUpdateData.terminate_thread  = 0;
    textureUpdateData.updatedTexture    = 0;
    textureUpdateData.imageDisplayData  = &displayData;
    textureUpdateData.arousal           = &moodDetectionData.arousal_prediction;
    textureUpdateData.valence           = &moodDetectionData.valence_prediction;

    HANDLE  handle_mood;
	DWORD   threadId_mood;

	HANDLE  handle_textureUpdate;
	DWORD   threadId_textureUpdate;

	int     i;

	printInfo();

    /* Initialize feature extraction information */
    printf( "Initializing Feature extraction ...\n" );
    fe_initialize_extraction_info( &extraction_info );

    /* Initialize arrays for feature extraction */
    portAudioData = fe_initialize_extraction_thread_data( &extraction_info );
    if( !portAudioData.init_success )
    {
        fprintf( stderr, "There was a problem initializing the feature extraction process\n  Exiting...\n" );
        return -1;
    }

    /* Initialize models for mood prediction and set up thread data */
    printf( "Initializing Mood detection models ...\n" );
    moodDetectionData = mr_initialize_mood_detection_data( &extraction_info, portAudioData );
    if( moodDetectionData.init_success == 0 )
    {
        fprintf( stderr, "There was a problem initializing the mood detection models\n  Exiting...\n" );
        fe_clean_extraction_thread_data( &portAudioData );
        return -1;
    }

    /* Initialize SDL */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
	{
		fprintf( stderr, "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
		fe_clean_extraction_thread_data( &portAudioData );
		mr_clean_mood_detection_data( &moodDetectionData );
		return -1;
	}

    /* Initialize portaudio and set device information */
    printf( "Initializing PortAudio ...\n" );

    err = Pa_Initialize();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while initializing PortAudio\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );

        SDL_Quit();
        fe_clean_extraction_thread_data( &portAudioData );
		mr_clean_mood_detection_data( &moodDetectionData );
		return -1;
    }

    /* Gather avialable device information */
    numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 )
    {
        printf( "ERROR: Pa_CountDevices returned 0x%x\n", numDevices );
        goto error;
    }

    /* Sort devices into input and output devices */
    input_list_num = (int*)malloc( sizeof(int) * numDevices );
    output_list_num = (int*)malloc( sizeof(int) * numDevices );

    for( i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
        if( deviceInfo->maxInputChannels >= NUM_CHANNELS )
        {
            *(input_list_num + numInputDevices) = i;
            numInputDevices++;
        }
        if( deviceInfo->maxOutputChannels >= NUM_CHANNELS )
        {
            *(output_list_num + numOutputDevices) = i;
            numOutputDevices++;
        }
    }
    if( numInputDevices == 0 )
    {
        printf( "ERROR: No input devices found\n" );
        goto error;
    }

    /* Set up input stream parameters */
    /* Select input device */
    printf( "\n Available input devices:\n" );
    for( i=0; i<numInputDevices; i++ )
    {
        printf( "\t%d: %s\n", i+1, Pa_GetDeviceInfo( input_list_num[i] )->name );
    }
    printf( "\n Enter input device number: " );
    chosenDeviceNum = getuint();
    while( ( chosenDeviceNum < 1 ) ||
           ( chosenDeviceNum > numInputDevices ) )
    {
        printf( "   Invalid input, try again: " );
        chosenDeviceNum = getuint();
    }

    inputParameters.device = input_list_num[ chosenDeviceNum - 1 ];
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;

    /* Set up output parameters and open stream */
    /* Select output device */
    printf( "\n CAUTION: Choosing an output device may cause feedback\n" );
    printf( " Available output devices:\n" );
    printf( "\t0: Do not use an output device\n" );
    for( i=0; i<numOutputDevices; i++ )
    {
        printf( "\t%d: %s\n", i+1, Pa_GetDeviceInfo( output_list_num[i] )->name );
    }
    printf( "\n Enter output device number: " );
    chosenDeviceNum = getuint();
    while( ( chosenDeviceNum < 0 ) ||
           ( chosenDeviceNum > numOutputDevices ) )
    {
        printf( "   Invalid input, try again: " );
        chosenDeviceNum = getuint();
    }

    if( chosenDeviceNum != 0 )
    {
        portAudioData.boolOutputDevice = 1;

        outputParameters.device = output_list_num[ chosenDeviceNum - 1 ];
        outputParameters.channelCount = NUM_CHANNELS;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.hostApiSpecificStreamInfo = NULL;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    }

    /* Initialize image display */
    printf( "\nInitalizing image display ...\n" );
    displayData = id_initialize_imageDisplay_data();
    if( displayData.init_success == 0 )
    {
        fprintf( stderr, "There was a problem initializing the image display\n" );
        goto error;
    }

    /* Open PortAudio stream */
    if( chosenDeviceNum == 0 )  /* User chose not to use an output device */
    {
        err = Pa_OpenStream(&stream,
                            &inputParameters,
                            NULL,
                            extraction_info.fs,
                            extraction_info.frame_length,
                            paClipOff,
                            paCallBack,
                            &portAudioData);
        if( err != paNoError )
            goto error;
    }
    else
    {
        err = Pa_OpenStream(&stream,
                            &inputParameters,
                            &outputParameters,
                            extraction_info.fs,
                            extraction_info.frame_length,
                            paClipOff,
                            paCallBack,
                            &portAudioData);
        if( err != paNoError )
            goto error;
    }

    /* Start stream */
    printf( "\nStarting stream ...\n" );
    err = Pa_StartStream( stream );
    if( err != paNoError )
        goto error;

    /* Start mood detection and texture updating threads */
    handle_mood = CreateThread( NULL,
                                0,
                                MoodDetectionRoutine,
                                &moodDetectionData,
                                0,
                                &threadId_mood );

    handle_textureUpdate = CreateThread( NULL,
                                         0,
                                         id_textureUpdateRoutine,
                                         &textureUpdateData,
                                         0,
                                         &threadId_textureUpdate );

    if( ( handle_mood == NULL ) || ( handle_textureUpdate == NULL ) )
    {
        fprintf( stderr, "Error starting threads\n" );
        printf( "\n\nExiting...\n" );
    }
    else
    {

        printf( "\n  To stop program, exit out of Image Processing window\n\n");

        /* Handle image processing and wait for exit event */
        Uint8           alpha = 255;    /* Transparency */
        SDL_Event       e;              /* Event handler */
        int             quit = 0;
        id_texture_info swapTexture;

        /* While quit hasn't been given */
        while( quit != 1 )
        {
            /* Handle events on queue */
            while( SDL_PollEvent( &e ) != 0 )
            {
                /* User requests quit */
                if( e.type == SDL_QUIT )
                    quit = 1;
            }

            /* Update transparency and/or background and foreground images */
            /* Set new transparency */
            alpha -= 4;
            if( alpha < 4 )
            {
                while( !textureUpdateData.updatedTexture ); /* Wait until texture has been updated in image display thread to swap */
                alpha = 255;
                swapTexture = displayData.texture_foreground;
                displayData.texture_foreground = displayData.texture_background;
                displayData.texture_background = displayData.texture_waiting;
                displayData.texture_waiting = swapTexture;  /* Will be updated the image display thread */

                /* Reset the blending mod of the swapped out texture */
                if( SDL_SetTextureAlphaMod( displayData.texture_waiting.texture, (Uint8)255 ) < 0 )
                    printf( " Error setting texture mod\n" );

                textureUpdateData.updatedTexture = 0;       /* Lets thread continue texture updating after swap is complete */
            }

            if( SDL_SetTextureAlphaMod( displayData.texture_foreground.texture, alpha ) < 0 )
                printf( "\n Error setting texture mod\n" );

            /* Clear screen */
            SDL_RenderClear( displayData.renderer );

            /* Render background texture */
            if( SDL_RenderCopy( displayData.renderer,
                                displayData.texture_background.texture,
                                NULL,
                                NULL ) < 0 )
                fprintf( stderr, "There was an error copying texture! SDL Error: %s\n", SDL_GetError() );

            /* Render foreground texture */
            if( SDL_RenderCopy( displayData.renderer,
                                displayData.texture_foreground.texture,
                                NULL,
                                NULL ) < 0 )
                fprintf( stderr, "There was an error copying texture! SDL Error: %s\n", SDL_GetError() );

            /* Update screen */
            SDL_RenderPresent( displayData.renderer );
        }

        printf( "\n\nExiting...\n" );

        moodDetectionData.terminate_thread = 1;
        textureUpdateData.terminate_thread = 1;
        textureUpdateData.updatedTexture = 0;       /* Set to zero so texture updating thread does not hang */

        WaitForSingleObject( handle_mood, 10000 );
        WaitForSingleObject( handle_textureUpdate, 10000 );
    }

    /* Clean up */
    err = Pa_StopStream( stream );
    if( err != paNoError )
        goto error;

    err = Pa_CloseStream( stream );
    if( err != paNoError )
        goto error;

    err = Pa_Terminate();
    if( err != paNoError )
        goto error;

    SDL_Quit();
    id_clean_imageDisplay_data( &displayData );
    mr_clean_mood_detection_data( &moodDetectionData );
    fe_clean_extraction_thread_data( &portAudioData );
    free(input_list_num);
    free(output_list_num);

    return err;

    /* Clean when error */
error:
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using PortAudio\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    }

    Pa_Terminate();
    if( displayData.init_success == 1)
        id_clean_imageDisplay_data( &displayData );
    SDL_Quit();
    mr_clean_mood_detection_data( &moodDetectionData );
    fe_clean_extraction_thread_data( &portAudioData );
    free(input_list_num);
    free(output_list_num);

    return err;
}


int getuint( void )
{
    const int MAX_DIGITS = 4;

    char    input[MAX_DIGITS + 1];
    char    ch;

    int     num;
    int     goodInput = -1;
    int     i;

    input[MAX_DIGITS] = '\0';

    for( i=0; i<MAX_DIGITS; i++ )
    {
        ch = getchar();
        if( isdigit(ch) )
            input[i] = ch;
        else
        {
            if( ch == '\n' )
            {
                if( i > 0 )
                {
                    goodInput = 1;
                    input[i] = '\0';
                    break;
                }
                else
                {
                    goodInput = 0;
                    break;
                }
            }
            else
            {
                while( getchar() != '\n' );
                goodInput = 0;
                break;
            }
        }
    }

    if( goodInput == -1 )
    {
        ch = getchar();
        if( ch == '\n' )
            goodInput = 1;
        else
        {
            goodInput = 0;
            while( getchar() != '\n' );
        }
    }

    if( goodInput == 1 )
    {
        num = atoi(input);
        return num;
    }
    else
        return -1;

}

void printInfo( void )
{
    printf( "\n***********************************************************************\n\n"

            "                        Musical Mood Detector\n"
            "                            and Visualizer\n\n"

            " Copyright (c) 2017 Jay Biernat\n"
            " Copyright (c) 2017 University of Rochester\n\n"

            " This program is free software: you can redistribute it and/or modify\n"
            " it under the terms of the GNU General Public License as published by\n"
            " the Free Software Foundation, either version 3 of the License, or\n"
            " (at your option) any later version.\n\n"

            " This program is distributed in the hope that it will be useful, but\n"
            " WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
            " General Public License for more details.\n\n"

            "***********************************************************************\n\n" );
    Sleep( 3000 );

    return;
}
