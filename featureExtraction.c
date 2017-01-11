/* featureExtraction.c Contains definitions of function relevant to audio feature extraction
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
#include <fftw3.h>
#include <math.h>
#include <portaudio.h>
#include "featureExtraction.h"

#ifndef PI
#define PI 3.1415926536
#endif // PI

int fe_compare( const void *a, const void *b )
{
    return ( *(float*)a > *(float*)b ) - ( *(float*)a < *(float*)b );
}

/******************************************************/

float fe_mean( float *values, int num )
{
    int     i;
    float   sum = 0;

    for( i=0; i<num; i++ )
        sum += *(values+i);

    return sum / (float)num;
}

/******************************************************/

float fe_stdv( float *values, int num )
{
    int     i;
    float   sum = 0;
    float   mean = fe_mean( values, num );

    for( i=0; i<num; i++ )
        sum += (float)pow( (double)( *(values+i) - mean ), 2 );

    return (float)sqrt( (double)( sum / (float)(num-1) ) );
}

/****************************************************************/

void fe_hamming_win( float *win, int N )
{
    int i;

    for( i=1; i<(N+1); i++ )
        *(win+i-1) = 0.54 - 0.46 * cos(2*PI*i/N);
}

/****************************************************************/

void fe_compute_magnitude( fftwf_complex *fft, float *magnitude, int N )
{
    int     i;
    float   a, b;

    for( i=0; i<N; i++ )
    {
        a = *(*fft);     /* Dereferencing fft returns a pointer to a float (the real part of the fftwf_complex type) */
        b = *(*fft+1);   /* Increment the pointer to the float to access the imaginary part of the fftwf_complex type */
        *magnitude = (float)sqrt( (double)( pow((double)a,2) + pow((double)b,2) ) );
        fft++;
        magnitude++;
    }
}

/******************************************************/

void fe_autocorrelate( float *x, float *ac, int N )
{
    int     i, j;
    float   sum;

    for( i=0; i<(int)(N/2); i++ )
    {
        sum = 0;
        for( j=0; j<N; j++ )
        {
            if( i+j < N )
                sum += (*(x+j)) * (*(x+j+i));
            else
                sum += (*(x+j)) * (*(x+j+i-N));
        }
        *(ac+i) = sum;
    }
}

/******************************************************/

void fe_timbre_stats( float *feature_array, float *stat_array, fe_extraction_info *info )
{
    int i;
    int rows =      info->num_timbre_features;
    int columns =   info->frames_in_window;

    for( i=0; i<rows; i++ )
    {
        *(stat_array) =     fe_mean( (feature_array + i*columns), columns );
        *(stat_array+1) =   fe_stdv( (feature_array + i*columns), columns );
        stat_array += 2;
    }
}

/******************************************************/

float fe_spectral_rolloff( float *magnitude, fe_extraction_info *info )
{
    if( info->rolloff<0 || info->rolloff>1 )
        return -1;    /* rolloff must be between zero and one */

    float   temp = 0;
    float   sum = 0;
    int     i;

    for( i=0; i<info->dft_length; i++ )
    {
        sum += *magnitude;
        magnitude++;
    }

    magnitude -= info->dft_length;     /* rewind pointer */
    i = 0;
    do
    {
        temp += *magnitude;
        magnitude++;
        i++;
    } while( temp/sum < info->rolloff );

    return (float)i*(float)info->fs/(float)info->frame_length;
}

/******************************************************/

float fe_spectral_centroid( float *magnitude, fe_extraction_info *info )
{
    int     i;
    float   fs_f = (float)info->fs;
    float   N_f = (float)info->frame_length;
    float   mag_sum = 0;
    float   scaled_mag_sum = 0;

    for( i=0; i<info->dft_length; i++ )
    {
        mag_sum += *magnitude;
        scaled_mag_sum += (float)i*fs_f/N_f * (*magnitude);
        magnitude++;
    }

    return scaled_mag_sum/mag_sum;
}

/******************************************************/

void fe_spectral_contrast( float *magnitude, float *contrast_features, fe_extraction_info *info )
{
    int     i, j;
    float   a = 0.2;      /* Neighborhood factor */
    int     neighborhood;
    float   peak, valley;

    int     boundary[ info->bands+1 ];
    float   mag_cpy[ info->dft_length ];
    float   *mag_cpy_ptr = mag_cpy;

    /* Calculate indices of band boundaries */
    boundary[0] = 0;
    for( i=1; i<(info->bands+1); i++ )
        boundary[i] = ( info->frame_length / pow( 2,(double)(info->bands-(i-1)) ) ) - 1;

    /* Copy array because we don't want to sort the original magnitude array */
    for( i=0; i<info->dft_length; i++ )
        mag_cpy[i] = *(magnitude+i);

    /* For each band */
    for( i=0; i<info->bands; i++ )
    {
        /* Sort band */
        qsort( mag_cpy_ptr, boundary[i+1]-boundary[i], sizeof(float), fe_compare );

        /* Calculate valley, peak, and contrast */
        neighborhood = a * (boundary[i+1] - boundary[i]);       /* Number of points in neighborhood */
        peak = 0;
        valley = 0;
        for( j=0; j<neighborhood; j++ )
        {
            valley +=   *( mag_cpy_ptr + j );
            peak +=     *( mag_cpy_ptr + (boundary[i+1]-boundary[i]-neighborhood) + j );
        }
        valley /=   (float)neighborhood;
        peak /=     (float)neighborhood;

        /* Store peak, valley, and contrast in contrast_features array */
        *contrast_features =                                           (float)log( (double)peak );
        *(contrast_features + info->bands*info->frames_in_window) =    (float)log( (double)valley );
        *(contrast_features + info->bands*2*info->frames_in_window) =  (float)log( (double)(peak-valley) );
        contrast_features += info->frames_in_window;

        /* Move arrayPtr to start of next band */
        mag_cpy_ptr = mag_cpy + boundary[i+1];
    }

    return;
}

/************************************************************************/

float fe_spectral_flux( float               *mag_cur,
                        float               *mag_prev,
                        fe_spec_flux_t      fluxType,
                        fe_extraction_info  *info )
{
    int     i;
    float   sum = 0;

    if( fluxType == FE_SPEC_FLUX_UNRECTIFIED )
    {
        for( i=0; i<info->dft_length; i++ )
            sum += (float)pow( (double)(*(mag_cur+i) - *(mag_prev+i)) , 2 );

    }else{
        float flux;

        for( i=0; i<info->dft_length; i++ )
        {
            flux = *(mag_cur+i) - *(mag_prev+i);
            flux = ( flux + (float)fabs( (double)flux ) ) / 2;  /* zero if flux from prev line in negative */
            sum += (float)pow( (double)flux, 2 );
        }
    }

    return sum / (float)info->dft_length;
}

/************************************************************/

void fe_rhythmic_features( float *flux_buffer, float *rhythm_features, fe_extraction_info *info )
{
    int     i, j;
    int     onset_counter = 0;
    int     ac_peak_counter = 0;
    int     leftpoint, rightpoint;
    int     ac_length = (int)(info->frames_in_window / 2);

    float   ac[ ac_length ];    /* holds autocorrelation curve */
    float   sum_onset_amp = 0;
    float   sum_ac_peaks = 0;
    float   sum_ac_valleys = 0;
    float   min;


    /* Calculate onsets per second and average height of onsets */
    float threshold = fe_mean( flux_buffer, info->frames_in_window ) + fe_stdv( flux_buffer, info->frames_in_window );
    for( i=1; i<(info->frames_in_window-1); i++)
    {
        if( ( *(flux_buffer+i) > *(flux_buffer+i-1) ) &&
            ( *(flux_buffer+i) > *(flux_buffer+i+1) ) &&
            ( *(flux_buffer+i) > threshold ) )
        {
            sum_onset_amp += *(flux_buffer+i);
            onset_counter++;
        }
    }

    /* Save onsets per sec and average onset height in rhymthm_features array */
    *rhythm_features =      (float)onset_counter / info->window_length;
    *(rhythm_features+1) =  sum_onset_amp / (float)onset_counter;

    /* Calculate average autocorrelation peak and valley strength */
    fe_autocorrelate( flux_buffer, ac, info->frames_in_window );
    threshold = fe_mean( (ac+1), ac_length-1 ) + fe_stdv( (ac+1), ac_length-1 );    /* Ignore very first large peak in ac */

    for( i=2; i<ac_length-1; i++ )  /* Again ignoring first large peak */
    {
        if( ( *(ac+i) > *(ac+i-1) ) &&
            ( *(ac+i) > *(ac+i+1) ) &&
            ( *(ac+i) > threshold ) )
        {
            sum_ac_peaks += *(ac+i);
            ac_peak_counter++;

            if( ac_peak_counter == 1 )
                leftpoint = i;
            else
            {
                rightpoint = i;
                min = *(ac + leftpoint);
                for( j=1; j<(rightpoint - leftpoint); j++ ) /* Search between peaks for valley */
                {
                    if( *(ac + leftpoint + j) < min )
                        min = *(ac + leftpoint + j);
                }
                sum_ac_valleys += min;
                leftpoint = rightpoint;
            }
        }
    }

    /* Save average ac peaks and valleys heights in array */
    *(rhythm_features+2) =  sum_ac_peaks / (float)ac_peak_counter;
    *(rhythm_features+3) =  (ac_peak_counter > 1) ? sum_ac_valleys / (float)(ac_peak_counter - 1) : sum_ac_peaks;

    return;
}

/*********************************************************/

void fe_initialize_extraction_info( fe_extraction_info *info )
{
    info->fs =               FS;
    info->frame_length =     N_SAMPS;
    info->dft_length =       (int)(N_SAMPS/2+1);

    info->window_length =    3.0;        /* In seconds */
    info->frames_in_window = ( info->window_length*info->fs -
                             ((int)info->window_length*info->fs % info->frame_length) ) /
                             info->frame_length;

    info->rolloff =          ROLLOFF;
    info->bands =            BANDS;

    info->num_timbre_features =  NUM_TIMBRE_FEATURES;
    info->num_onset_features =   NUM_ONSET_FEATURES;

    return;
}

/*********************************************************/

fe_extraction_thread_data fe_initialize_extraction_thread_data( fe_extraction_info *info )
{
    int i;
    fe_extraction_thread_data thread_data;

    thread_data.info =                  info;    /* pointer to extraction info */
    thread_data.columnPtr =             0;       /* column index counter */

    thread_data.audio = NULL;
    thread_data.hamm_win = NULL;
    thread_data.dft = NULL;
    thread_data.magnitude = NULL;
    thread_data.prev_mag = NULL;
    thread_data.rectified_flux_buffer = NULL;
    thread_data.timbre_matrix = NULL;
    thread_data.fftPlan = NULL;

    thread_data.audio = (float*)fftwf_malloc( sizeof(float) * info->frame_length );
    if( thread_data.audio == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.hamm_win = (float*)malloc( sizeof(float) * info->frame_length );
    if( thread_data.hamm_win == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }
    fe_hamming_win( thread_data.hamm_win, info->frame_length );   /* Fill hamming window array */

    thread_data.dft = (fftwf_complex*)fftwf_malloc( sizeof(fftwf_complex) * info->dft_length );
    if( thread_data.dft == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.magnitude = (float*)malloc( sizeof(float) * info->dft_length );
    if( thread_data.magnitude == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.prev_mag = (float*)malloc( sizeof(float) * info->dft_length );
    if( thread_data.prev_mag == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }
    for( i=0; i<info->dft_length; i++ )     /* Set initial previous magnitude to zero */
        *(thread_data.magnitude + i) = 0;

    thread_data.rectified_flux_buffer = (float*)malloc( sizeof(float) * info->frames_in_window );
    if( thread_data.rectified_flux_buffer == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.timbre_matrix = (float*)malloc( sizeof(float) * info->frames_in_window * info->num_timbre_features );
    if( thread_data.timbre_matrix == NULL )
    {
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.fftPlan = fftwf_plan_dft_r2c_1d( info->frame_length, thread_data.audio, thread_data.dft, FFTW_MEASURE );
    if( thread_data.fftPlan == NULL )
    {
        printf( "Error: Could not create fftw plan\n" );
        thread_data.init_success = 0;
        goto exit;
    }

    thread_data.init_success = 1;
    thread_data.boolOutputDevice = 0;

exit:
    if( thread_data.init_success == 0 )
    {
        free(thread_data.timbre_matrix);
        fftwf_free(thread_data.audio);
        free(thread_data.hamm_win);
        fftwf_free(thread_data.dft);
        free(thread_data.magnitude);
        free(thread_data.prev_mag);
        free(thread_data.rectified_flux_buffer);

        thread_data.audio = NULL;
        thread_data.hamm_win = NULL;
        thread_data.dft = NULL;
        thread_data.magnitude = NULL;
        thread_data.prev_mag = NULL;
        thread_data.rectified_flux_buffer = NULL;
        thread_data.timbre_matrix = NULL;
    }

    return thread_data;
}

/**********************************************************/

void fe_clean_extraction_thread_data( fe_extraction_thread_data *thread_data )
{
    fftwf_destroy_plan(thread_data->fftPlan);

    fftwf_free(thread_data->audio);
    free(thread_data->hamm_win);
    fftwf_free(thread_data->dft);
    free(thread_data->magnitude);
    free(thread_data->prev_mag);
    free(thread_data->rectified_flux_buffer);
    free(thread_data->timbre_matrix);

    thread_data->fftPlan = NULL;

    thread_data->audio = NULL;
    thread_data->hamm_win = NULL;
    thread_data->dft = NULL;
    thread_data->magnitude = NULL;
    thread_data->prev_mag = NULL;
    thread_data->rectified_flux_buffer = NULL;
    thread_data->timbre_matrix = NULL;

    return;
}

/**********************************************************/

int paCallBack( const void                        *inputBuffer,
                void                              *outputBuffer,
                unsigned long                     framesPerBuffer,
                const PaStreamCallbackTimeInfo*   timeInfo,
                PaStreamCallbackFlags             statusFlags,
                void                              *userData)
{
    /* Case data passed through stream to our structure */
    fe_extraction_thread_data *data = (fe_extraction_thread_data*)userData;
    float *in =     (float*)inputBuffer;
    float *out =    (float*)outputBuffer;
    unsigned int     i;

    /* Output two input channels to two output channels and average channels to audio array */
    for( i=0; i<framesPerBuffer; i++ )
    {
        if( data->boolOutputDevice == 1 )
        {
            *out++ = *in;
            *out++ = *(in+1);
        }

        *(data->audio + i) = ( *in + *(in+1) ) / 2;
        *(data->audio + i) *= *(data->hamm_win + i);    /* Apply hamming window */
        in += 2;
    }

    /* Perform fft plan and compute magnitude */
    fftwf_execute( data->fftPlan );
    fe_compute_magnitude( data->dft, data->magnitude, data->info->dft_length );

    /* Fill current column of timbre matrix */
    /* Spectral Centroid */
    *( data->timbre_matrix + data->columnPtr ) = fe_spectral_centroid( data->magnitude, data->info );
    /* Spectral Flux */
    *( data->timbre_matrix + data->columnPtr + data->info->frames_in_window ) = fe_spectral_flux( data->magnitude, data->prev_mag, FE_SPEC_FLUX_UNRECTIFIED, data->info );
    /* Spectral Rolloff */
    *( data->timbre_matrix + data->columnPtr + 2*data->info->frames_in_window ) = fe_spectral_rolloff( data->magnitude, data->info );
    /* Spectral Contrast Features */
    fe_spectral_contrast( data->magnitude, ( data->timbre_matrix + data->columnPtr + 3*data->info->frames_in_window ), data->info );

    /* Update rectified flux buffer for onset feature extraction*/
    *( data->rectified_flux_buffer + data->columnPtr ) = fe_spectral_flux( data->magnitude, data->prev_mag, FE_SPEC_FLUX_RECTIFIED, data->info );

    /* Switch pointers for magnitude and prev_mag */
    float *temp = data->magnitude;
    data->magnitude = data->prev_mag;
    data->prev_mag = temp;

    (data->columnPtr)++;
    if( data->columnPtr == data->info->frames_in_window )
        data->columnPtr = 0;

    return 0;
}
