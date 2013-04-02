/* 
 * File:   orGfx.h
 * Author: fib
 *
 * Created on 26 December 2012, 12:22
 */

#ifndef ORGFX_H
#define	ORGFX_H

# include "orStd.h"
# include "orMath.h"

# ifdef _MSC_VER
#   define WINGDIAPI __declspec(dllimport)
#   define APIENTRY __stdcall
#   define CALLBACK __stdcall
# endif

# include <GL/gl.h>
# include <GL/glu.h>

# ifdef _MSC_VER
#   undef WINGDIAPI
#   undef APIENTRY
#   undef CALLBACK
# endif


# include <Eigen/OpenGLSupport>

#endif	/* ORGFX_H */

