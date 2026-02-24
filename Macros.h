#pragma once

#define CELL_FLOATING         -4
#define CELL_SAFE             -3
#define CELL_FLAG             -2
#define CELL_UNDISCOVERED     -1
#define CELL_NUMBER(x)        (x)

#define CELLSTATE_DISABLED    -2
#define CELLSTATE_UNPREDICTED -1
#define CELLSTATE_DETERMINED   1
#define CELLSTATE_UNDETERMINED 0

#define RELATION_SUBSET -1
#define RELATION_EQUAL 0
#define RELATION_SUPERSET 1
#define RELATION_DISJOINT -2
#define RELATION_JOINT 2

#define MAX_ENDGAME_CONFIGS 100
#define MAX_ENDGAME_CELLS 64