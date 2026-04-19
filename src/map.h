#ifndef MAP_H
#define MAP_H

#define MAX_BOXES 1024

#include "raylib.h"

typedef struct {
    Vector3 p1, p2, p3; // 3 points defining the plane
    char texture[64];
    float offset_x, offset_y;
    float rotation;
    float scale_x, scale_y;
} BrushFace;

typedef struct {
    BrushFace faces[64];
    int face_count;
} Brush;

typedef struct {
    Brush brushes[1024];
    int brush_count;
} MapEntity;

typedef struct Box {
    Vector3 position;
    Vector3 size;
    char texture_name[20];
} Box;

typedef struct Map {
    Box boxes[MAX_BOXES];
    int box_count;
} Map;

#endif // !MAP_H
