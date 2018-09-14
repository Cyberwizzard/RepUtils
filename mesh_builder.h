/*
 * mesh_builder.h
 *
 *  Created on: Sept 14, 2018
 *      Author: cyberwizzard
 */

#ifndef MESH_BUILDER_H_
#define MESH_BUILDER_H_


//#define KEY_DOWN 258
//#define KEY_UP 259
//#define KEY_LEFT 260
//#define KEY_RIGHT 261
#define KEY_F1 265
#define KEY_F2 266
#define KEY_F3 267
#define KEY_F4 268
#define KEY_F5 269
#define KEY_F6 270
#define KEY_F7 271
#define KEY_F8 272
#define KEY_F9 273
#define KEY_F10 274
#define KEY_F11 275
#define KEY_F12 276

typedef struct {
	float x;	  // X location of the mesh point
	float y;	  // Y location of the mesh point

	float z;	  // Offset from the Z home point, always negative if the bed is below the home point

	int   valid;  // Mark which points have been filled as valid
} ty_meshpoint;


int mesh_builder();


#endif /* MESH_BUILDER_H_ */
