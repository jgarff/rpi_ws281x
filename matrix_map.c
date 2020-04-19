/*
 * matrix_map.c
 *
 * Copyright (c) 2020 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "matrix_map.h"


typedef struct matrix_map {
    int width;
    int height;
    int type;
    uint32_t *map;
} matrix_map_t;

#define MATRIX_MAP_INDEX(matrix_mapptr, x, y)    ((matrix_mapptr->width * y) + x)


/**
 * Display the matrix for debugging purposes.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
void matrix_map_show(matrix_map_t *matrix) {
    int x, y;

    for (y = 0; y < matrix->height; y++) {
        for (x = 0; x < matrix->width; x++) {
            int index = MATRIX_MAP_INDEX(matrix, x, y);

            printf(" %-4d", matrix->map[index]);
        }
        printf("\n");
    }
}

/**
 * Initialize the matrix map for a zigzag type array
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
void matrix_map_init_zigzag(matrix_map_t *matrix) {
    int x, y;

    for (y = 0; y < matrix->height; y++) {
        for (x = 0; x < matrix->width; x++) {
            int index = MATRIX_MAP_INDEX(matrix, x, y);

            if (y % 2 == 0) {     // Even lines
                matrix->map[index] = index;
            } else {              // Odd lines
                matrix->map[index] = ((matrix->width * y) - 1) + (matrix->width - x);
            }
        }
    }
}

/**
 * Initialize the matrix map for a normal left to right
 * style matrix.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
void matrix_map_init_normal(matrix_map_t *matrix) {
    int x, y;

    for (y = 0; y < matrix->height; y++) {
        for (x = 0; x < matrix->width; x++) {
            int index = MATRIX_MAP_INDEX(matrix, x, y);

            matrix->map[index] = index;
        }
    }
}

/**
 * Initialize the matrix map.
 *
 * @param    width    Width of the matrix in the X direction.
 * @param    height   Height of the matrix in tye Y direction.
 * @param    type     MATRIX_TYPE_NORMAL or MATRIX_TYPE_ZIGZAG
 *
 * @returns  None
 */
matrix_map_t *matrix_map_init(int width, int height, int type) {
    matrix_map_t *matrix;

    matrix = malloc(sizeof(*matrix));
    if (!matrix) {
        return NULL;
    }

    matrix->width = width;
    matrix->height = height;
    matrix->type = type;

    matrix->map = malloc(sizeof(uint32_t) * width * height);
    if (!matrix->map) {
        return NULL;
    }

    switch (type) {
        case MATRIX_TYPE_NORMAL:
            matrix_map_init_normal(matrix);
            break;

        case MATRIX_TYPE_ZIGZAG:
            matrix_map_init_zigzag(matrix);
            break;

        default:
            return NULL;
    }

    return matrix;
}

/**
 * Free the matrix.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
void matrix_map_free(matrix_map_t *matrix) {
    if (matrix->map) {
        free(matrix->map);
    }

    if (matrix) {
        free(matrix);
    }
}

/**
 * Mirror the matrix in the X (width) direction.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
void matrix_map_mirror_x(matrix_map_t *matrix) {
    uint32_t map[matrix->width * matrix->height];
    int x, y;

    for (y = 0; y < matrix->height; y++) {
        for (x = 0; x < matrix->width; x++) {
            int oldindex = MATRIX_MAP_INDEX(matrix, x, y);
            int newindex = MATRIX_MAP_INDEX(matrix, matrix->width - x - 1, y);

            map[newindex] = matrix->map[oldindex];
        }
    }

    memcpy(matrix->map, map, sizeof(map));
}

/**
 * Rotate the matrix by 90 degress in the clockwise direction.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
int matrix_map_rotate_90(matrix_map_t *matrix) {
    uint32_t map[matrix->width * matrix->height];
    int x, y;

    // Must be same width and height
    if (matrix->width != matrix->height) {
        return -1;
    }

    for (y = 0; y < matrix->height; y++) {
        for (x = 0; x < matrix->width; x++) {
            int oldindex = MATRIX_MAP_INDEX(matrix, x, y);
            int newindex = MATRIX_MAP_INDEX(matrix, matrix->width - y - 1, x);

            map[newindex] = matrix->map[oldindex];
        }
    }

    memcpy(matrix->map, map, sizeof(map));

    return 0;
}

/**
 * Given a x and y coordinate, return the string index.
 *
 * @param    matrix  Maxtrix pointer.
 *
 * @returns  None
 */
int matrix_map_index(matrix_map_t *matrix, int x, int y) {
    int index;
    
    if (x > matrix->width) {
        return -1;
    }
    
    if (y > matrix->height) {
        return -1;
    }

    index = MATRIX_MAP_INDEX(matrix, x, y);

    return matrix->map[index];
}

