/* moodRecognition.c Defines functions used in mood recognition via SVR model
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

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <math.h>
#include <string.h>
#include "moodRecognition.h"
#include "featureExtraction.h"

mr_detection_thread_data mr_initialize_mood_detection_data( fe_extraction_info *extractionInfo , fe_extraction_thread_data portAudioData )
{
    const char                  arousalDirectory[] = "..\\assets\\arousal.info";
    const char                  valenceDirectory[] = "..\\assets\\valence.info";
    mr_detection_thread_data    moodDetectionData;

    moodDetectionData.init_success = 0;

    moodDetectionData.arousal_mdl = mr_create_model( arousalDirectory );
    moodDetectionData.valence_mdl = mr_create_model( valenceDirectory );

    /* Mood detection features are the mean and std deviation of each timbre feature and the onset features */
    moodDetectionData.features = (float*)malloc( sizeof(float) * ( extractionInfo->num_timbre_features * 2 + extractionInfo->num_onset_features ) );

    /* If there was a problem creating a model or allocating feature memory, free resources and return */
    if( moodDetectionData.features == NULL ||
        !moodDetectionData.valence_mdl.init_success ||
        !moodDetectionData.arousal_mdl.init_success )
    {
        mr_destroy( &moodDetectionData.arousal_mdl );
        mr_destroy( &moodDetectionData.valence_mdl );
        free( moodDetectionData.features );

        moodDetectionData.features = NULL;

        return moodDetectionData;
    }

    moodDetectionData.arousal_prediction =  0;
    moodDetectionData.valence_prediction =  0;
    moodDetectionData.terminate_thread =    0;
    moodDetectionData.timbre_matrix =       portAudioData.timbre_matrix;
    moodDetectionData.rec_flux_buffer =     portAudioData.rectified_flux_buffer;
    moodDetectionData.extraction_info =     extractionInfo;

    moodDetectionData.init_success =        1;

    return moodDetectionData;
}

/************************************************************/

void mr_clean_mood_detection_data( mr_detection_thread_data *thread_data )
{
    mr_destroy( &thread_data->valence_mdl );
    mr_destroy( &thread_data->arousal_mdl );
    free( thread_data->features );

    thread_data->features = NULL;
}

/************************************************************/

mr_array mr_fill_array( const char *path )
{
    int         i = 0;
    mr_array   array_struct;
    FILE        *filePtr;
    char        header[10];
    float       *arrayPtr;

    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        printf( "Error: Could not open file %s\n", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }

    /* Check header for COLS and get data */
    fscanf( filePtr, "%s", header );
    if( strcmp( header, "COLS" ) )          /* strcmp() will return 0 if the strings are equal */
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }
    if( fscanf( filePtr, "%d", &array_struct.N )!= 1 )
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }

    /* Check header for ROWS and get data */
    fscanf( filePtr, "%s", header );
    if( strcmp( header, "ROWS" ) )          /* strcmp() will return 0 if the strings are equal */
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }
    if( fscanf( filePtr, "%d", &array_struct.M )!= 1 )
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }

    /* Check header for DATA and then allocate memory for array */
    fscanf( filePtr, "%s", header );
    if( strcmp( header, "DATA" ) )          /* strcmp() will return 0 if the strings are equal */
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }
    /* Allocate memory needed for data */
    array_struct.data_ptr = (float*) malloc( sizeof(float) * array_struct.N * array_struct.M );
    if( array_struct.data_ptr == NULL )
    {
        printf(" Error: Problem in data file %s", path );
        array_struct.data_ptr = NULL;
        goto exit;
    }
    /* Store data */
    arrayPtr = array_struct.data_ptr;
    for( i=0; i<array_struct.N * array_struct.M; i++ )
    {
        if( fscanf( filePtr, "%f", arrayPtr ) != 1 )
        {
            printf(" Error: Problem in data file %s", path );
            free(array_struct.data_ptr);
            array_struct.data_ptr = NULL;
            break;
        }
        arrayPtr++;
    }
    /* Check to see if extra data beyond expected amount is in file */
    if( i == array_struct.N * array_struct.M )
    {
        float dummyFloat;
        if( fscanf( filePtr, "%f", &dummyFloat ) == 1 )
        {
            printf(" Error: Problem in data file %s", path );
            free(array_struct.data_ptr);
            array_struct.data_ptr = NULL;
        }
    }

exit:
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    return array_struct;
}

/***************************************************/

void mr_free_array( mr_array *thisArray )
{
    free(thisArray->data_ptr);
    thisArray->data_ptr = NULL;
    thisArray->M = 0;
    thisArray->N = 0;
}

/***************************************************/

mr_model mr_create_model( const char *directory )
{
    mr_model    mdl;
    mr_array    returned_array;
    char        path[200];
    FILE        *filePtr;

    mdl.mu = NULL;
    mdl.sigma = NULL;
    mdl.alpha = NULL;
    mdl.support_vectors = NULL;

    /* Fill in scale and bias by reading single float entry from the respective text files */

    /* Bias */
    strcpy( path, directory );
    strcat( path, "\\bias.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( fscanf( filePtr, "%f", &mdl.bias ) != 1 )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    /* Scale */
    strcpy( path, directory );
    strcat( path, "\\scale.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( fscanf( filePtr, "%f", &mdl.scale )!= 1 )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    /*Fill in other arrays using mr_array_fill */

    /* Send path to mu data file to mr_fill_array */
    strcpy( path, directory );
    strcat( path, "\\mu.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    returned_array = mr_fill_array( path );
    if( returned_array.data_ptr == NULL )       /* Make sure data was read in correctly */
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( returned_array.M != 1 )                 /* Numbers of rows must be 1 */
    {
        mdl.init_success = 0;
        mr_free_array( &returned_array );       /* Only free array memory on this type of error */
        goto exit;
    }
    mdl.num_features = returned_array.N;
    mdl.mu = returned_array.data_ptr;
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    /* Send path to sigma data file to mr_fill_array */
    strcpy( path, directory );
    strcat( path, "\\sigma.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    returned_array = mr_fill_array( path );
    if( returned_array.data_ptr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( (returned_array.M != 1) || (returned_array.N != mdl.num_features) )
    {
        mdl.init_success = 0;
        mr_free_array( &returned_array );
        goto exit;
    }
    mdl.sigma = returned_array.data_ptr;
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    /* Send path to alpha data file to mr_fill_array */
    strcpy( path, directory );
    strcat( path, "\\alpha.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    returned_array = mr_fill_array( path );
    if( returned_array.data_ptr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( (returned_array.M != 1) )
    {
        mdl.init_success = 0;
        mr_free_array( &returned_array );
        goto exit;
    }
    mdl.num_sv = returned_array.N;
    mdl.alpha = returned_array.data_ptr;
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    /* Send path to support_vectors data file to mr_fill_array */
    strcpy( path, directory );
    strcat( path, "\\support_vectors.txt" );
    filePtr = fopen( path, "r" );
    if( filePtr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    returned_array = mr_fill_array( path );
    if( returned_array.data_ptr == NULL )
    {
        mdl.init_success = 0;
        goto exit;
    }
    if( (returned_array.N != mdl.num_features) || (returned_array.M != mdl.num_sv) )
    {
        mdl.init_success = 0;
        mr_free_array( &returned_array );
        goto exit;
    }
    mdl.support_vectors = returned_array.data_ptr;

    mdl.init_success = 1;

exit:
    if( mdl.init_success == 0 )
    {
        free(mdl.mu);
        free(mdl.sigma);
        free(mdl.alpha);
        mdl.mu = NULL;
        mdl.sigma = NULL;
        mdl.alpha = NULL;
    }
    if( fclose(filePtr) )
        printf( "Error:  Could not close file %s\n", path );

    return mdl;
}

/***************************************************/

void mr_destroy( mr_model *mdl )
{
    free(mdl->mu);
    free(mdl->sigma);
    free(mdl->alpha);
    free(mdl->support_vectors);

    mdl->mu =               NULL;
    mdl->sigma =            NULL;
    mdl->alpha =            NULL;
    mdl->support_vectors =  NULL;

    return;
}

/***************************************************/

float mr_predict( float *x, mr_model mdl )
{
    int     i, j;
    float   sum = 0;
    float   norm_sum = 0;
    float   x_transformed;
    float   x_normed[mdl.num_features];

    if( mdl.init_success != 1 )
        return 0;

    float varience = (float)pow( (double)mdl.scale, 2 );

    /* Normalize features */
    for( i=0; i<mdl.num_features; i++ )
        x_normed[i] = ( *(x+i) - *( mdl.mu + i ) ) / *( mdl.sigma + i );

    /* For each support vector */
    for( i=0; i<mdl.num_sv; i++ )
    {
        norm_sum = 0;
        /* Transform with Gaussian kernel */
        for( j=0; j<mdl.num_features; j++ )
            norm_sum += (float)pow( (double)( *( mdl.support_vectors + i*mdl.num_features + j ) - x_normed[j] ) , 2 );
        x_transformed = (float)exp( (double)( -norm_sum / varience ) );

        sum += ( *( mdl.alpha + i ) * x_transformed );
    }

    return sum + mdl.bias;
}

/********************************************************/

unsigned int __stdcall MoodDetectionRoutine(void *lpArg)
{
    mr_detection_thread_data *threadData = (mr_detection_thread_data*)lpArg;

    Sleep( 3500 );

    while( !(threadData->terminate_thread) )
    {
        fe_timbre_stats( threadData->timbre_matrix, threadData->features, threadData->extraction_info );
        /* Arithmetic in second argument calculates the starting point of the rhythmic features in the features array */
        fe_rhythmic_features( threadData->rec_flux_buffer, (threadData->features + threadData->extraction_info->num_timbre_features * 2), threadData->extraction_info );

        threadData->arousal_prediction = mr_predict( threadData->features, threadData->arousal_mdl );
        threadData->valence_prediction = mr_predict( threadData->features, threadData->valence_mdl );
    }

    _endthreadex( 0 );
	return 0;
}

