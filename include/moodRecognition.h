/* moodRecognition.h Defines structures and declares functions used in mood recognition via SVR model
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

#ifndef MOODRECOGNITION_H_INCLUDED
#define MOODRECOGNITION_H_INCLUDED

#include <windows.h>
#include "featureExtraction.h"

/******************* Structures *******************/

/** Contains the support vectors and relevant information of a trained SVR model */
typedef struct
{
    int     init_success;   /* Set to 1 for successful initializtion, 0 otherwise */

    int     num_features;
    int     num_sv;         /* Number support vectors */

    float   *mu;            /* Feature normalization */
    float   *sigma;

    float   scale;         /* For Gaussian kernel and predictor function */
    float   bias;
    float   *alpha;
    float   *support_vectors;
}
mr_model;

/** Contains pointer to and dimensions of a two-dimensional float array */
typedef struct
{
    float   *data_ptr;
    int     N;      /* Num columns or num elements in 1d array */
    int     M;      /* Num rows */
}
mr_array;

/** Pointer to this data structure to be passed to the mood recognition thread upon thread's creation */
typedef struct
{
    int     terminate_thread;   /* Flag for thread termination */
    int     init_success;       /* Set to 0 if initializtion of structure failed, 1 on success */

    fe_extraction_info *extraction_info;    /* Pointer to structure containing feature extraction information */

    float   *timbre_matrix;     /* Two dimensional array where each row is a timbre feature and each column is a frame of audio */
    float   *rec_flux_buffer;   /* Circular buffer holding past frame's rectified spectral flux values */
    float   *features;          /* Feature vector used by SVR model */

    mr_model arousal_mdl;       /* Trained SVR models used to predict arousal and valence of a section of audio */
    mr_model valence_mdl;

    float   arousal_prediction;
    float   valence_prediction;
}
mr_detection_thread_data;

/****************** Functions ******************/

/** @brief Initializes and returns the data structure to be passed to the mood recognition thread

    @param extractionInfo An initialized fe_extraction_info structure initialized by fe_initialize_extrantion_info()
    @param poartAudioData The initialized fe_initialize_extraction_thread_data to be used by the port audio callback function

    @return A mr_detection_thread_data structure to be passed (via a void pointer) to the mood recognition thread.  Strucutre
    member init_success is set to 0 on failure of initialization and 1 on success.

*/
mr_detection_thread_data mr_initialize_mood_detection_data( fe_extraction_info *extractionInfo , fe_extraction_thread_data portAudioData );

/** @brief Frees memory in a mr_detection_thread_data structure initalized by mr_initialize_mood_detection_data().
    Must be before the end of the program when a successful call to mr_initialize_mood_detection_data() has
    been made

    @param thread_data Pointer to the mr_detection_thread_data structure to be cleaned up
*/
void mr_clean_mood_detection_data( mr_detection_thread_data *thread_data );

/** @brief Fills an mr_array structure with data from a specially formatted text files.  Memory in an mr_array object
    can be freed by mr_free_array

    @param path A path to a specially formatted text file with data to fill an mr_array

    @return An mr_array initialized with data from the provided file.  The memory pointed to by data_ptr within
    the structure must be freed at a later point.  Structure member data_ptr will be set to NULL on failure
    to initialize mr_array
*/
mr_array mr_fill_array( const char *path );

/** @brief Frees memory used in an mr_array sturcture

    @param A poniter to the mr_array to be freed
*/
void mr_free_array( mr_array *thisArray );

/** @brief Initializes an mr_model with information necessary to do predictions with a trained SVR model.
    mr_destroy() must be called after a successful call to mr_create_model().

    @param directory A path to a directory with specially formated files containing the data of the trained SVR model

    @return An mr_model structure holding the data relevant to a trained SVR model. Strucutre
    member init_success is set to 0 on failure of initialization and 1 on success.
*/
mr_model mr_create_model( const char *directory );

/** @brief Frees memory allocated in a mr_model structure

    @param mdl Pointer to the mr_model to free allocated memory from
*/
void mr_destroy( mr_model *mdl );

/** @brief Makes a prediction given an array of features and a trained SVR model

    @param x Pointer to an array of floats that are the features to be used for the SVR prediction
    @param mdl An SVR model used in the prediction

    @return The predicted value returned by the SVR
*/
float mr_predict( float *x, mr_model mdl );

/** @brief The callback funtion used by the texture updating thread

    @param lpArg A pointer cast as LPVOID that points to a mr_detection_thread_data structure
*/
DWORD WINAPI MoodDetectionRoutine(LPVOID lpArg);

#endif // MOODRECOGNITION_H_INCLUDED
