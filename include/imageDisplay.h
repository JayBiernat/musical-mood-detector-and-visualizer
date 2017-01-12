/* imageDisplay.h Defines structures and declares functions used in texture updating and image display
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

/* imageDisplay.h
 *
 * Author: Jay Biernat
 * Last Edited: 1/05/17
 *
 * Defines structures used for updating image and handling image display
 * Declares image processing and display functions (defined in imageDisplay.c)
 * Declares callback function used in texture updating thread (also defined in imageDisplay.c)
 *
 */

#ifndef IMAGEDISPLAY_H_INCLUDED
#define IMAGEDISPLAY_H_INCLUDED

#include <SDL.h>
#include <windows.h>
#include "moodRecognition.h"


/********************** Defines *****************************/


/** Forgetting factor when updating the arousal and valence values used in the determining
    the saturation and brightness in the displayed image.

    @see id_textureUpdateRoutine
*/
#define LAMBDA 0.5


/*********************** Structures *************************/


/** Contains hue, saturation, and value (brightness) values for a single pixel */
typedef struct
{
    float h;
    float s;
    float v;
}
id_hsvPixel;

/** Contains pointer to a texture and relevant information needed for processing and updating the texture */
typedef struct
{
    SDL_Texture *texture;
    void        *pixels;    /* Pointer to texture's pixel data */
    Uint32      format;     /* A pixel format (specified by the SDL library) of the texture's pixels */
    int         pitch;      /* Length of a row of pixels in bytes */
    int         h;          /* Height of texture (in pixels) */
    int         w;          /* Width of texture (in pixels) */
}
id_texture_info;

/** Contains data relevant to displaying and processing the image/texutres on screen */
typedef struct
{
    int             init_success;

    SDL_Window      *window;                /* The window being rendered to */
    SDL_Renderer    *renderer;              /* The surface contained by the window */
    id_texture_info texture_updating;       /* Texture being updated */
    id_texture_info texture_waiting;        /* Texture waiting to be used on screen */
    id_texture_info texture_foreground;     /* Foreground texture */
    id_texture_info texture_background;     /* Background texture */

    id_hsvPixel        *hsvPixelData;          /* Pointer to a the pixel data from the original,
                                                unmodified image */
}
id_imageDisplay_data;

/** Pointer to this data structure to be passed to the texture updating thread upon thread's creation

    terminate_thread and updatedTexture are used to flag the thread to terminate or update and are
    changed by the process's main thread
*/
typedef struct
{
    int                     terminate_thread;   /* Flag for thread termination */
    int                     updatedTexture;     /* Flag to signal thread to update the texture */
    id_imageDisplay_data    *imageDisplayData;  /* Data structure needed for texture updating and dispaly */

    float                   *arousal;           /* Pointer to current arousal (used to determine saturation */
    float                   *valence;           /* Pointer to current valence *used to determine value/brightness */
}
id_textureThreadStruct;


/*********************** Functions *************************/


/** @brief Initializes an id_imageDisplay_data structure to be used by the program. Creates SDL objects
    (window, renderer, surface, etc.) used by the SDL library for image display/. Loads a user-specified
    (or the default) image and saves original pixel information for later use

    @return A structure with information relevent to the SDL API for updating and displaying a chsoen
    image.  On failure, the structure is returned with all pointers set to NULL (including all pointers
    in the id_texture_info structures contained within the id_imageDisplay_data structure), and the
    integer init_succes is set to zero.  On success, init_success is set to 1.
*/
id_imageDisplay_data id_initialize_imageDisplay_data( void );


/** @brief Free memory in a id_imageDiplsay_data structure initalized by id_initialize_imageDisplay_data().
    Must be before the end of the program when a successful call to id_initialize_imageDisplay_data has
    been made

    @param display_data Pointer to the id_imageDisplay_data structure to be cleaned up
*/
void id_clean_imageDisplay_data( id_imageDisplay_data *display_data );

/** @brief Updates a texture by changing the brightness and saturation of the image based on the
    provided valence and arousal values.

    @param texture Pointer to the id_texture_info structure of the texture to be updated
    @param format Pointer to the SDL_PixelFormat of the window returned by SDL_GetWindowSurface()
    @param hsvPtr Pointer to the HSV pixel data of the original, unmodified image
    @param arousal Floating point value between -1 and 1 used in determining image saturation
    @param valence Floating point value between -1 and 1 used in determining image value/brightness
*/
void id_updateTexture( id_texture_info *texture,
                       const struct SDL_PixelFormat *format,
                       id_hsvPixel *hsvPtr,
                       float arousal,
                       float valence );

/** @brief The callback funtion used by the texture updating thread

    @param lpArg A pointer cast as LPVOID that points to a id_textureThreadStruct structure
*/
DWORD WINAPI id_textureUpdateRoutine(LPVOID lpArg);

/** @brief Returns the mininum of three floating point arguments */
float minOfThree( float a, float b, float c );

/** @brief Returns the maximum of three floating point arguments */
float maxOfThree( float a, float b, float c );

/** @brief Converts RGB pixel values into the HSV color space

    @param r_int An integer from 0 to 255 representing the red value of an RGB pixel
    @param g_int An integer from 0 to 255 representing the green value of an RGB pixel
    @param b_int An integer from 0 to 255 representing the blue value of an RGB pixel

    @param h Pointer to a float where the hue value of the pixel will be stored
    @param s Pointer to a float where the saturation value of the pixel will be stored
    @param v Pointer to a float where the value/brightness value of the pixel will be stored
*/
void RGBtoHSV( int r_int, int g_int, int b_int, float *h, float *s, float *v );

/** @brief Converts a pixel's color from HSV color space into RGB values

    @param r Pointer to an int where the red value of the pixel will be stored
    @param g Pointer to an int where the green value of the pixel will be stored
    @param b Pointer to an int where the blue value of the pixel will be stored

    @param h A float between 0 and 1 representing the hue value of an HSV pixel
    @param s A float between 0 and 1 representing the saturation value of an HSV pixel
    @param v A float between 0 and 1 representing the value/brightness value of an HSV pixel.
    If s == 0, then h == -1 (undefined)

*/
void HSVtoRGB( int *r, int *g, int *b, float h, float s, float v );


#endif // IMAGEPROCESSING_H_INCLUDED
