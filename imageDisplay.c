/* imageDisplay.c Defines functions used in texture updating and image display
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

/*
 * Color conversion algorithm and original code for HSVtoRGB and
 * RGBtoHSV functions taken from:
 *   https://www.cs.rit.edu/~ncs/color/t_convert.html
 *   Written by: Eugene Vishnevsky
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include "imageDisplay.h"

id_imageDisplay_data id_initialize_imageDisplay_data( void )
{
    const int NUM_PATH_CHARS = 200;

    id_imageDisplay_data     imageDisplay_data;
    const char defaultPath[] = "..\\assets\\flower.bmp";
    char chosenPath[NUM_PATH_CHARS];

    SDL_Window          *window = NULL;            /* The window to be rendered to */
    SDL_Surface         *convertedSurface = NULL;  /* The surface converted to the window's pixel format */
    SDL_Surface         *BMPSurface = NULL;        /* Loaded BMP image */
    SDL_Renderer        *renderer = NULL;          /* Texture Renderer */
    id_hsvPixel         *hsvPixelData = NULL;      /* Points to image's original pixels converted to HSV color space */
    id_texture_info     texture_updating;          /* Four textures used in updating and image display */
    id_texture_info     texture_waiting;
    id_texture_info     texture_foreground;
    id_texture_info     texture_background;

    texture_updating.texture =      NULL;
    texture_updating.pixels =       NULL;
    texture_waiting.texture =       NULL;
    texture_waiting.pixels =        NULL;
    texture_foreground.texture =    NULL;
    texture_foreground.pixels =     NULL;
    texture_background.texture =    NULL;
    texture_background.pixels =     NULL;

    Uint32      *surfacePtr =           NULL;
    Uint32      *textureUpdatingPtr =   NULL;
    Uint32      *textureWaitingPtr =    NULL;
    Uint32      *textureBackgroundPtr = NULL;
    Uint32      *textureForegroundPtr = NULL;
    id_hsvPixel *hsvPtr =               NULL;

    int         i, j;                       /* Counters */
    Uint8       r, g, b;                    /* Values for RGB color space */
    float       h, s, v;                    /* Values for HSV colorspace */

    /* Load image as an SDL surface, attempting loads until success or the deafult image fails to load */
    while( 1 )
    {
        printf( "\n Enter full path to desired BMP image or enter nothing to use default image: " );
        if( fgets( chosenPath, NUM_PATH_CHARS, stdin ) != NULL )
        {
            if( *chosenPath == '\n' )
            {
                BMPSurface = SDL_LoadBMP( defaultPath );
                if( BMPSurface == NULL )
                {
                    fprintf( stderr, "\n  Unable to load image default image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );

                    imageDisplay_data.init_success = 0;
                    goto exit;
                }
                else
                    break;
            }
            else
            {
                for( i=0; i<NUM_PATH_CHARS; i++ )
                {
                    if( *(chosenPath + i) == '\n' )
                    {
                        *(chosenPath + i) = '\0';  /* Replace new line character with EOS character so we can use SDL_LoadBMP() */
                        break;
                    }
                }
                if( i == NUM_PATH_CHARS )
                {
                    printf("\n  WARNING: User entered path is too long (Maximum path characters is %d)\n", NUM_PATH_CHARS-1 );
                    while( getchar() != '\n' );
                }

                BMPSurface = SDL_LoadBMP( chosenPath );
                if( BMPSurface == NULL )
                    fprintf( stderr, "\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
                else
                    break;
            }
        }
        else
        {
            fprintf( stderr, "ERROR: There was a problem reading user input\n" );

            imageDisplay_data.init_success = 0;
            goto exit;
        }
    }

    /* Create window */
    window = SDL_CreateWindow( "Image Processing",
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               BMPSurface->w,
                               BMPSurface->h,
                               SDL_WINDOW_SHOWN );
    if( window == NULL )
    {
        printf( "ERROR: Window could not be created! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }

    /* Create renderer for window */
    renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
    if( renderer == NULL )
    {
        printf( "ERROR: Renderer could not be created! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }

    /* Create surface formatted for window from loaded BMP surface */
    convertedSurface = SDL_ConvertSurface( BMPSurface, SDL_GetWindowSurface( window )->format, 0 );
    if( convertedSurface == NULL )
    {
        fprintf( stderr, "ERROR: Unable to convert surface to window format! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }

    /* Create 4 new streaming textures (lockable for updating pixel information) */
    /* Texture for updating */
    texture_updating.texture = SDL_CreateTexture( renderer, SDL_GetWindowPixelFormat( window ), SDL_TEXTUREACCESS_STREAMING, convertedSurface->w, convertedSurface->h );
    if( texture_updating.texture == NULL )
    {
        fprintf( stderr, "ERROR: Unable to create blank texture! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }
    texture_updating.format = SDL_GetWindowPixelFormat( window );
    texture_updating.h = convertedSurface->h;
    texture_updating.w = convertedSurface->w;
    if( SDL_SetTextureBlendMode( texture_updating.texture, SDL_BLENDMODE_BLEND ) < 0 )
        printf( " WARNING: Error setting texture blend mode\n" );

    /* Texture waiting for displaying on screen */
    texture_waiting.texture = SDL_CreateTexture( renderer, SDL_GetWindowPixelFormat( window ), SDL_TEXTUREACCESS_STREAMING, convertedSurface->w, convertedSurface->h );
    if( texture_waiting.texture == NULL )
    {
        fprintf( stderr, "ERROR: Unable to create blank texture! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }
    texture_waiting.format = SDL_GetWindowPixelFormat( window );
    texture_waiting.h = convertedSurface->h;
    texture_waiting.w = convertedSurface->w;
    if( SDL_SetTextureBlendMode( texture_waiting.texture, SDL_BLENDMODE_BLEND ) < 0 )
        printf( " WARNING: Error setting texture blend mode\n" );

    /* Texture for background blitting */
    texture_background.texture = SDL_CreateTexture( renderer, SDL_GetWindowPixelFormat( window ), SDL_TEXTUREACCESS_STREAMING, convertedSurface->w, convertedSurface->h );
    if( texture_background.texture == NULL )
    {
        fprintf( stderr, "ERROR: Unable to create blank texture! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }
    texture_background.format = SDL_GetWindowPixelFormat( window );
    texture_background.h = convertedSurface->h;
    texture_background.w = convertedSurface->w;
    if( SDL_SetTextureBlendMode( texture_background.texture, SDL_BLENDMODE_BLEND ) < 0 )
        printf( " WARNING: Error setting texture blend mode\n" );

    /* Texture for foreground blitting */
    texture_foreground.texture = SDL_CreateTexture( renderer, SDL_GetWindowPixelFormat( window ), SDL_TEXTUREACCESS_STREAMING, convertedSurface->w, convertedSurface->h );
    if( texture_foreground.texture == NULL )
    {
        fprintf( stderr, "ERROR: Unable to create blank texture! SDL Error: %s\n", SDL_GetError() );

        imageDisplay_data.init_success = 0;
        goto exit;
    }
    texture_foreground.format = SDL_GetWindowPixelFormat( window );
    texture_foreground.h = convertedSurface->h;
    texture_foreground.w = convertedSurface->w;
    if( SDL_SetTextureBlendMode( texture_foreground.texture, SDL_BLENDMODE_BLEND ) < 0 )
        printf( " WARNING: Error setting texture blend mode\n" );

    /* Copy pixel data from converted surface to all textures */
    /* Convert to HSV color space and save in array for future use */
    hsvPixelData = (id_hsvPixel*)malloc( sizeof(id_hsvPixel) * convertedSurface->w * convertedSurface->h );
    if( hsvPixelData == NULL )
    {
        fprintf( stderr, "ERROR: Not enough memory for HSV pixel array\n" );

        imageDisplay_data.init_success = 0;
        goto exit;
    }

    /* Lock all textures in order to update their pixel information */
    SDL_LockTexture( texture_updating.texture, NULL, &texture_updating.pixels, &texture_updating.pitch );
    SDL_LockTexture( texture_waiting.texture, NULL, &texture_waiting.pixels, &texture_waiting.pitch );
    SDL_LockTexture( texture_background.texture, NULL, &texture_background.pixels, &texture_background.pitch );
    SDL_LockTexture( texture_foreground.texture, NULL, &texture_foreground.pixels, &texture_foreground.pitch );

    /* Copy and typecast pointers to make the copying of pixel information below easier */
    surfacePtr =            (Uint32*)convertedSurface->pixels;
    textureUpdatingPtr =    (Uint32*)texture_updating.pixels;
    textureWaitingPtr =     (Uint32*)texture_waiting.pixels;
    textureBackgroundPtr =  (Uint32*)texture_background.pixels;
    textureForegroundPtr =  (Uint32*)texture_foreground.pixels;
    hsvPtr =                hsvPixelData;

    for( i=0; i<convertedSurface->h; i++ )
    {
        for( j=0; j<convertedSurface->w; j++ )
        {
            /* Copy pixel data to all four textures */
            /* Note: in the SDL API, pitch is the number of pixels between the beginning of each row of pixels - NOT ALWAYS THE SAME AS THE PIXEL WIDTH */
            *( textureUpdatingPtr + (i*(texture_updating.pitch / 4)) + j ) = *( surfacePtr + (i*(convertedSurface->pitch / 4)) + j );
            *( textureWaitingPtr + (i*(texture_waiting.pitch / 4)) + j ) = *( surfacePtr + (i*(convertedSurface->pitch / 4)) + j );
            *( textureBackgroundPtr + (i*(texture_background.pitch / 4)) + j ) = *( surfacePtr + (i*(convertedSurface->pitch / 4)) + j );
            *( textureForegroundPtr + (i*(texture_foreground.pitch / 4)) + j ) = *( surfacePtr + (i*(convertedSurface->pitch / 4)) + j );

            /* Copy and convert RGB data to HSV array */
            SDL_GetRGB( *( surfacePtr + (i*(convertedSurface->pitch / 4)) + j ),
                        convertedSurface->format,
                        &r,
                        &g,
                        &b );
            RGBtoHSV( (int)r, (int)g, (int)b, &h, &s, &v );
            hsvPtr->h = h;
            hsvPtr->s = s;
            hsvPtr->v = v;

            hsvPtr++;
        }
    }

    /* Unlock textures after accessing pixel information and reset pixels and pitch values when not in use */
    SDL_UnlockTexture( texture_updating.texture );
    texture_updating.pixels = NULL;
    texture_updating.pitch = 0;
    SDL_UnlockTexture( texture_waiting.texture );
    texture_waiting.pixels = NULL;
    texture_waiting.pitch = 0;
    SDL_UnlockTexture( texture_background.texture );
    texture_background.pixels = NULL;
    texture_background.pitch = 0;
    SDL_UnlockTexture( texture_foreground.texture );
    texture_foreground.pixels = NULL;
    texture_foreground.pitch = 0;

    imageDisplay_data.init_success = 1;     /* Successfully initialized all data */

    /* Clear screen */
    SDL_SetRenderDrawColor( renderer, 0xFF, 0xFF, 0xFF, 0xFF );
    SDL_RenderClear( renderer );

    /* Render updated texture */
    if( SDL_RenderCopy( renderer,
                        texture_foreground.texture,
                        NULL,
                        NULL ) < 0 )
        fprintf( stderr, "ERRROR: There was an error copying texture! SDL Error: %s\n", SDL_GetError() );

    /* Update screen */
    SDL_RenderPresent( renderer );

exit:
    if( imageDisplay_data.init_success == 0 )
    {
        /* Destroy any objets that may have been created and free any allocated memory */
        SDL_DestroyTexture( texture_updating.texture );
        SDL_DestroyTexture( texture_waiting.texture );
        SDL_DestroyTexture( texture_foreground.texture );
        SDL_DestroyTexture( texture_background.texture );
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_FreeSurface( convertedSurface );
        SDL_FreeSurface( BMPSurface );
        free( hsvPixelData );

        imageDisplay_data.window =                      NULL;
        imageDisplay_data.texture_updating.texture =    NULL;
        imageDisplay_data.texture_updating.pixels =     NULL;
        imageDisplay_data.texture_waiting.texture =     NULL;
        imageDisplay_data.texture_waiting.pixels =      NULL;
        imageDisplay_data.texture_foreground.texture =  NULL;
        imageDisplay_data.texture_foreground.pixels =   NULL;
        imageDisplay_data.texture_background.texture =  NULL;
        imageDisplay_data.texture_background.pixels =   NULL;
        imageDisplay_data.hsvPixelData =                NULL;
    }
    else
    {
        imageDisplay_data.window =              window;
        imageDisplay_data.renderer =            renderer;
        imageDisplay_data.texture_updating =    texture_updating;
        imageDisplay_data.texture_waiting =     texture_waiting;
        imageDisplay_data.texture_background =  texture_background;
        imageDisplay_data.texture_foreground =  texture_foreground;
        imageDisplay_data.hsvPixelData =        hsvPixelData;

        /* Free original BMP and converted surfaces, no longer need them */
        SDL_FreeSurface( convertedSurface );
        SDL_FreeSurface( BMPSurface );
    }

    return imageDisplay_data;
}

/********************************************************************/

void id_clean_imageDisplay_data( id_imageDisplay_data *display_data )
{
    /* Destroy textures, renderer, and window */
    SDL_DestroyTexture( display_data->texture_updating.texture );
    display_data->texture_updating.texture = NULL;
    SDL_DestroyTexture( display_data->texture_waiting.texture );
    display_data->texture_waiting.texture = NULL;
    SDL_DestroyTexture( display_data->texture_background.texture );
    display_data->texture_background.texture = NULL;
    SDL_DestroyTexture( display_data->texture_foreground.texture );
    display_data->texture_foreground.texture = NULL;
    SDL_DestroyRenderer( display_data->renderer );
    display_data->renderer = NULL;
    SDL_DestroyWindow( display_data->window );
    display_data->window = NULL;

    /* Free memory */
    free( display_data->hsvPixelData );
    display_data->hsvPixelData = NULL;

    return;
}

/*******************************************************************/

void id_updateTexture( id_texture_info *texture,
                       const struct SDL_PixelFormat *format,
                       id_hsvPixel *hsvPtr,
                       float arousal,
                       float valence )
{
    Uint32  *pixelPtr;
    Uint32  rgbPixel;
    float   beta, gamma;      /* Saturation and value modifiers */
    float   s_temp, v_temp;
    int     r, g, b;
    int     i, j;

    /* Saturation modifier */
    if( arousal > 0 )
        beta = 1 - arousal;
    else
        beta = 1 / (1 + arousal);

    /* Value (brightness) modifier */
    if( valence > 0 )
        gamma = 1 - valence;
    else
        gamma = 1 / (1 + valence);

    /* Lock texture to acces pixel information and update */
    if( SDL_LockTexture( texture->texture, NULL, &(texture->pixels), &(texture->pitch) ) < 0 )
    {
        fprintf( stderr, "Unable to lock texture!\n" );
        return;
    }
    pixelPtr = (Uint32*)texture->pixels;

    for( i=0; i<texture->h; i++ )
    {
        for( j=0; j<texture->w; j++ )
        {
            /* Modify the saturation and brightness values of the original pixel and convert to RGB values */
            s_temp = (float)pow( (double)hsvPtr->s, (double)beta );
            v_temp = (float)pow( (double)hsvPtr->v, (double)gamma );

            HSVtoRGB( &r, &g, &b, hsvPtr->h, s_temp, v_temp );

            r = (r > 255) ? 255 : r;
            g = (g > 255) ? 255 : g;
            b = (b > 255) ? 255 : b;

            rgbPixel = SDL_MapRGB( format, (Uint8)r, (Uint8)g, (Uint8)b );
            *( pixelPtr + i*(texture->pitch / 4) + j ) = rgbPixel;

            hsvPtr++;
        }
    }

    /* Unlock texture */
    SDL_UnlockTexture( texture->texture );
    texture->pixels = NULL;
    texture->pitch = 0;

    return;
}

/******************************************************************/

float minOfThree( float a, float b, float c )
{
    float minimum = a;
    if( b < minimum )
        minimum = b;
    if( c < minimum )
        minimum = c;

    return minimum;
}

/******************************************************************/

float maxOfThree( float a, float b, float c )
{
    float maximum = a;
    if( b > maximum )
        maximum = b;
    if( c > maximum )
        maximum = c;

    return maximum;
}

/******************************************************************/

/* int r,g,b values need to be converted to floats from 0 to 1
 * h = [0,360], s = [0,1], v = [0,1]
 *		if s == 0, then h = -1 (undefined)
 */
void RGBtoHSV( int r_int, int g_int, int b_int, float *h, float *s, float *v )
{
    float r, g, b;
	float minimum, maximum, delta;

	r = (float)r_int/255;
	g = (float)g_int/255;
	b = (float)b_int/255;

	minimum = minOfThree( r, g, b );
	maximum = maxOfThree( r, g, b );
	*v = maximum;
	delta = maximum - minimum;
	if( maximum != 0 && delta != 0 )
		*s = delta / maximum;
	else {
		/* r = g = b = 0		s = 0, h is undefined */
		*s = 0;
		*h = -1;
		return;
	}
	if( r == maximum )
		*h = ( g - b ) / delta;		/* between yellow & magenta */
	else if( g == maximum )
		*h = 2 + ( b - r ) / delta;	/* between cyan & yellow */
	else
		*h = 4 + ( r - g ) / delta;	/* between magenta & cyan */
	*h *= 60;				        /* degrees */
	if( *h < 0 )
		*h += 360;

    return;
}

/******************************************************************/

void HSVtoRGB( int *r, int *g, int *b, float h, float s, float v )
{
	int i;
	float f, p, q, t;
	if( s == 0 )
    {
		/* achromatic (grey) */
		*r = (int)(v * 255);
        *g = (int)(v * 255);
        *b = (int)(v * 255);
		return;
	}

	h /= 60;			        /* sector 0 to 5 */
	i = (int)h;
	f = h - (float)i;			/* factorial part of h */
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );
	switch( i ) {
		case 0:
			*r = (int)(v * 255);
			*g = (int)(t * 255);
			*b = (int)(p * 255);
			break;
		case 1:
			*r = (int)(q * 255);
			*g = (int)(v * 255);
			*b = (int)(p * 255);
			break;
		case 2:
			*r = (int)(p * 255);
			*g = (int)(v * 255);
			*b = (int)(t * 255);
			break;
		case 3:
			*r = (int)(p * 255);
			*g = (int)(q * 255);
			*b = (int)(v * 255);
			break;
		case 4:
			*r = (int)(t * 255);
			*g = (int)(p * 255);
			*b = (int)(v * 255);
			break;
		default:		// case 5:
			*r = (int)(v * 255);
			*g = (int)(p * 255);
			*b = (int)(q * 255);
			break;
	}

	return;
}

/******************************************************************/

DWORD WINAPI id_textureUpdateRoutine(LPVOID lpArg)
{
    id_textureThreadStruct *threadData = (id_textureThreadStruct*)lpArg;
    id_texture_info swapTexture;

    float prev_arousal = 0;
    float prev_valence = 0;
    float cur_arousal, cur_valence;

	while( !( threadData->terminate_thread ) )
    {
        /* Update current arousal and valence */
        cur_arousal = ( (1-LAMBDA) * prev_arousal ) + ( LAMBDA * *(threadData->arousal) );
        cur_valence = ( (1-LAMBDA) * prev_valence ) + ( LAMBDA * *(threadData->valence) );

        /* Check for out-of-bounds and NaN/Inf */
        if( !(cur_arousal < 1) || !(cur_arousal > -1) )
            cur_arousal = prev_arousal;
        if( !(cur_valence < 1) || !(cur_valence > -1) )
            cur_valence = prev_valence;

        /* Update previous arousal and valence for future use */
        prev_arousal = cur_arousal;
        prev_valence = cur_valence;

        printf( "\tValence: %f\t Arousal: %f\r", cur_valence, cur_arousal );

        /* Scale current values for use in processing */
        cur_arousal *= 2;
        if( cur_arousal > 1)
            cur_arousal = 1;
        else if( cur_arousal < -1 )
            cur_arousal = -1;

        cur_valence *= 3;
        if( cur_valence > 1 )
            cur_valence = 1;
        else if( cur_valence < -1 )
            cur_valence = -1;

        /* Update a third texture not currently being used as the fore/background textures */
        id_updateTexture( &(threadData->imageDisplayData->texture_updating),
                          SDL_GetWindowSurface( threadData->imageDisplayData->window )->format,
                          threadData->imageDisplayData->hsvPixelData,
                          cur_arousal,
                          cur_valence );

        /* Wait until foreground texture has reached complete transparency to swap */
        while( threadData->updatedTexture );

        swapTexture = threadData->imageDisplayData->texture_waiting;
        threadData->imageDisplayData->texture_waiting = threadData->imageDisplayData->texture_updating;
        threadData->imageDisplayData->texture_updating = swapTexture;

        threadData->updatedTexture = 1;
    }

	return 0;
}
