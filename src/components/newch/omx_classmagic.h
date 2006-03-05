#ifndef OMX_CLASSMAGIC_H_
#define OMX_CLASSMAGIC_H_
/**
 * fixme disclaimer/legals
 * @file src/components/newch/omx_classmagic.h
 * @author Ukri Niemimuukko
 * 
 * This file contains class handling helper macros
 * It is left as an exercise to the reader how they do the magic (FIXME)
 * 
 * Usage Rules:
 * 1) include this file
 * 2) if your don't inherit, start your class with CLASS(classname)
 * 3) if you inherit something, start your class with 
 * 	DERIVEDCLASS(classname, inheritedclassname)
 * 4) end your class with ENDCLASS(classname)
 * 5) define your class variables with a #define classname_FIELDS inheritedclassname_FIELDS
 * 	inside your class and always add a backslash at the end of line (except last)
 * 6) if you want to use doxygen, use C-style comments inside the #define, and
 * 	enable macro expansion in doxyconf and predefine DOXYGEN_PREPROCESSING there, etc.
 * 
 * See examples at the end of this file (in #if 0 block)
 */


#ifdef DOXYGEN_PREPROCESSING
#define CLASS(a) class a { public:
#define DERIVEDCLASS(a, b) class a : public b { public:
#define ENDCLASS(a) a##_FIELDS };
#else
#define CLASS(a) typedef struct a {
#define DERIVEDCLASS(a, b) typedef struct a {
#define ENDCLASS(a) a##_FIELDS } a;
#endif


#if 0 /*EXAMPLES*/
/**
 * Class A is a nice class
 */
CLASS(A)
#define A_FIELDS \
/** @param a very nice parameter */ \
 	int a; \
/** @param ash another very nice parameter */ \
 	int ash;
ENDCLASS(A)
 
/**
 * Class B is a nice derived class
 */
DERIVEDCLASS(B,A)
#define B_FIELDS A_FIELDS \
/** @param b very nice parameter */ \
	int b;
ENDCLASS(B)

/**
 * Class B2 is a nice derived class
 */
DERIVEDCLASS(B2,A)
#define B2_FIELDS A_FIELDS \
/** @param b2 very nice parameter */ \
	int b2;
ENDCLASS(B2)

/**
 * Class C is an even nicer derived class
 */
DERIVEDCLASS(C,B)
#define C_FIELDS B_FIELDS \
/** @param c very nice parameter */ \
	int c;
ENDCLASS(C)

#endif /* 0 */

#endif /*OMX_CLASSMAGIC_H_*/
