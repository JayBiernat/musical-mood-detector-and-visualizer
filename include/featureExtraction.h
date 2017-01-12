/* featureExtraction.h Defines structures and declares functions used in audio feature extraction
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

#include <fftw3.h>
#include <portaudio.h>

#ifndef FEATUREEXTRACTION_H_INCLUDED
#define FEATUREEXTRACTION_H_INCLUDED

#define N_SAMPS 2048    /* samples per frame */
#define FS 44100
#define NUM_CHANNELS 2
#define ROLLOFF 0.85    /* Ratio for spectral rolloff */
#define BANDS 7
#define NUM_TIMBRE_FEATURES 24  /* Number of timbre and onset features used in SVR prediction */
#define NUM_ONSET_FEATURES 4

/** An enumberated type used as an argument in the function fe_spectral_flux() to indicate which type of
    spectral flux should be calculated
    @see fe_spectral_flux()
*/
typedef enum
{
    FE_SPEC_FLUX_UNRECTIFIED,
    FE_SPEC_FLUX_RECTIFIED
}
fe_spec_flux_t;

/******************** Structures **********************/

/** Structure holding relevant information needed to do feature extraction on audio */
typedef struct
{
    int     fs;             /* sampling frequency of the signal */
    int     frame_length;   /* number of samples in a frame */
    int     dft_length;     /* number of points in calculated dft (positive part of frequency spectrum) */

    float   window_length;      /* length (in seconds) of audio chunk used in mood prediction */
    int     frames_in_window;   /* number of frames that fit inside the length of the mood prediction window */

    int     bands;          /* number of bands used to calculate spectral contrast features */
    float   rolloff;        /* rolloff (float b/w zero and one) used to calculate spectral rolloff */

    int     num_timbre_features;    /* number of timbre features extracted from window */
    int     num_onset_features;     /* number of onset features extracted from window */
}
fe_extraction_info;

/** Structure to be passed via a void pointer to the paCallback function */
typedef struct
{
    float           *audio;         /* A frame of audio */
    float           *hamm_win;      /* Hamming window */
    fftwf_complex   *dft;           /* The DFT of a frame of audio */
    float           *magnitude;     /* The magnitude of the DFT */
    float           *prev_mag;
    float           *rectified_flux_buffer;
    float           *timbre_matrix;

    int             columnPtr;  /* Column index counter */

    fe_extraction_info  *info;
    fftwf_plan          fftPlan;    /* Plan used by FFTW API to calculate Fourier Transform */

    int     boolOutputDevice;       /* 1 if output device will be used, 0 otherwise */
    int     init_success;           /* 1 on structure's successful initialization, 0 otherwise */
}
fe_extraction_thread_data;

/************************** Functions ************************/

/** @brief The callback function used for qsort
    @see fe_spectral_contrast()
*/
int fe_compare( const void *a, const void *b);     /* Callback function for qsort */

/** @brief Returns the mean of an array of floats

    @param values Pointer to an array of floats
    @param num The number of floats in the array
    @return The mean of the array
*/
float fe_mean( float *values, int num );

/** @brief Returns the standard deviation of an array of floats

    @param values Pointer to an array of floats
    @param num The number of floats in the array
    @return The standard deviation of the array
*/
float fe_stdv( float *values, int num );

/** @brief Fill a float array with the values of a hamming window

    @param values Pointer to an array of floats that the hamming window values will be placed in
    @param N The number of points in the hamming window
*/
void fe_hamming_win( float *win, int N );

/** @brief Compute the magnitudes of an array of complex numbers

    @param fft Pointer to an array of fftwf_complex numbers whose magnitudes will be calculated
    @param magnitude Pointer to an array of floats where the magnitudes will be stored
    @param N The number of values in the fft array (which is also the number of values in the magnitude array)
*/
void fe_compute_magnitude( fftwf_complex *fft, float *magnitude, int N );

/** @brief Calculates the autocorrelation of an array of floats.

    @param flux_buffer A pointer to an array of floats representing a discrete signal for which the autocorrelation
    will be calculted
    @param ac A pointer to an array of floats where the autocorrelation will be placed.  The length of the autocorrelation
    will be N/2 rounded down
    @param N The length of the flux_buffer array
*/
void fe_autocorrelate( float *flux_buffer, float *ac, int N );

/** @brief Calculates the mean and standard deviation of a matrix of timbre features where the first two floats
    are the mean and stadard deviation of the first timbre feature, the next two floats are the mean and stadard
    deviation of the second timbre feature, etc.

    @param feature_array Pointer to the matrix of timbre features where each row is a timbre feature and each
    column is a frame of audio
    @param stat_array Pointer to an array of floats where the feature statistics will be placed
    @param info Pointer to an initialized fe_extraction_info structure
*/
void fe_timbre_stats( float *feature_array, float *stat_array, fe_extraction_info *info );

/** @brief Calculates spectral rolloff of a frequency spectrum

    @param magnitude Pointer to an array of floats holding the magnitude of a frequency spectrum
    @param info Pointer to an initilaized fe_extraction_info structure
    @return Spectral rolloff
*/
float fe_spectral_rolloff( float *magnitude, fe_extraction_info *info );

/** @brief Calcultes spectral centroid of a frequency spectrum

    @param magnitude Pointer to an array of floats holding the magnitude of a frequency spectrum
    @param info Pointer to an initilaized fe_extraction_info structure
    @return Spectral centroid
*/
float fe_spectral_centroid( float *magnitude, fe_extraction_info *info );

/** @brief Calcultes spectral contrast features

    @param magnitude Pointer to an array of floats holding the magnitude of a frequency spectrum
    @param contrast_features Pointer to the first element in the timbre feature matrix where the contrast
    features will be held in a part of a column representing one frame of audio
        first [bands] elements in the column are the peaks from each band
        second [bands] elements in the column are the valleys from each band
        third [bands] elements in the column are the contrasts of each band
    @param info Pointer to an initilaized fe_extraction_info structure
*/
void fe_spectral_contrast( float *magnitude, float *contrast_features, fe_extraction_info *info );

/** @brief Calculates spectral flux between two frequency spectrums

    @param mag_cur Pointer to array of the current frame's frequency spectrum magnitude
    @param mag_prev Pointer to array of the previous frame's frequency spectrum magnitude
    @param fluxType A fe_spec_flux_t value (either FE_SPEC_FLUX_UNRECTIFIED or FE_SPEC_FLUX_RECTIFIED)
    indicating which type of spectral flux should be calculated
    @param info Pointer to an initilaized fe_extraction_info structure
    @return Spectral Flux
*/
float fe_spectral_flux( float *mag_cur,
                        float *mag_prev,
                        fe_spec_flux_t fluxType,
                        fe_extraction_info *info );

/** @brief Calculates rhythmic features: onsets per second
                                         average height of onsets
                                         average autocorrelation peak
                                         average autocorrelation valley

    @param flux_buffer Pointer to a buffer of previous rectified spectral flux values
    @param rhythm_features Pointer to the starting element in the feature vector where the rhythm
    features will be stored
    @param info Pointer to an initilaized fe_extraction_info structure
*/
void fe_rhythmic_features( float *flux_buffer, float *rhythm_features, fe_extraction_info *info );

/** @brief Initialize the fe_extraction_info structure passed to it by pointer. Must call
    fe_clean_extraction_thread_data() to free memory from it
*/
void fe_initialize_extraction_info( fe_extraction_info *info );

/** @brief Initializes a fe_extraction_thread_data structure for use by the paCallback function

    @param info Pointer to an initilaized fe_extraction_info structure. The init_success member
    of the structure will be set to 1 on successful initializtion, and 0 on failure.
*/
fe_extraction_thread_data fe_initialize_extraction_thread_data( fe_extraction_info *info );

/** @brief Frees memory from the fe_extraction_thread_data passed to it by pointer */
void fe_clean_extraction_thread_data( fe_extraction_thread_data *thread_data );

/** @brief Callback function to be used by the PortAudio API in handling audio */
int paCallBack( const void                        *inputBuffer,
                void                              *outputBuffer,
                unsigned long                     framesPerBuffer,
                const PaStreamCallbackTimeInfo*   timeInfo,
                PaStreamCallbackFlags             statusFlags,
                void                              *userData );

#endif // FEATUREEXTRACTION_H_INCLUDED
