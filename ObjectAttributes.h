#ifndef OBJATTRIBUTES_H
#define OBJATTRIBUTES_H

typedef struct ObjectAttributes {
	uint16 attr0;
	uint16 attr1;
	uint16 attr2;
	uint16 pad;
} __attribute__((packed, aligned(4))) ObjectAttributes;

#endif