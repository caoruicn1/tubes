
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>

#define SCHEME_CREATE_MAIN
#define SCHEME_ASSERT_STDINOUT_ARE_PIPED
#define SCHEME_FUNCTION tesselate
#include "scheme.h"

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

int TEXTURE_WIDTH = 2000;
int TEXTURE_HEIGHT = 0;

struct Shape * final_shape = NULL;
struct Shape * out_shape = NULL;
void beginCallback(GLenum which)
{
  out_shape = new_shape();
  out_shape->gl_type = which;
  out_shape->vertex_arrays[0].num_dimensions = final_shape->vertex_arrays[0].num_dimensions;
}

void vertex3dv(const GLdouble * c)
{
  float v[2] = { c[0], c[1] };
  append_vertex(out_shape, v);
}

void endCallback(void)
{
  if (out_shape->gl_type == GL_TRIANGLE_STRIP)
  {
    struct VertexArray * va = get_or_add_array(out_shape, GL_VERTEX_ARRAY);
    
    int new_num_vertexs = ((out_shape->num_vertexs - 3) * 3) + 3;
    float * new_vertexs = (float*)malloc(sizeof(float)*va->num_dimensions*(new_num_vertexs));
    
    long i, j=0;
    for (i = 0 ; i < out_shape->num_vertexs ; i++)
    {
      if (j >= 3)
      {
        new_vertexs[j*va->num_dimensions+0] = va->vertexs[(i-2)*va->num_dimensions+0];
        new_vertexs[j*va->num_dimensions+1] = va->vertexs[(i-2)*va->num_dimensions+1];
        if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[(i-2)*va->num_dimensions+2];
        j++;
        new_vertexs[j*va->num_dimensions+0] = va->vertexs[(i-1)*va->num_dimensions+0];
        new_vertexs[j*va->num_dimensions+1] = va->vertexs[(i-1)*va->num_dimensions+1];
        if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[(i-1)*va->num_dimensions+2];
        j++;
      }
      new_vertexs[j*va->num_dimensions+0] = va->vertexs[i*va->num_dimensions+0];
      new_vertexs[j*va->num_dimensions+1] = va->vertexs[i*va->num_dimensions+1];
      if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[i*va->num_dimensions+2];
      j++;
    }
    
    free(va->vertexs);
    out_shape->num_vertexs = new_num_vertexs;
    va->vertexs = new_vertexs;
    out_shape->gl_type = GL_TRIANGLES;
  }
  else if (out_shape->gl_type == GL_TRIANGLE_FAN)
  {
    struct VertexArray * va = get_or_add_array(out_shape, GL_VERTEX_ARRAY);
    
    int new_num_vertexs = ((out_shape->num_vertexs - 3) * 3) + 3;
    float * new_vertexs = (float*)malloc(sizeof(float)*va->num_dimensions*(new_num_vertexs));
    
    long i, j=0;
    for (i = 0 ; i < out_shape->num_vertexs ; i++)
    {
      if (j >= 3)
      {
        new_vertexs[j*va->num_dimensions+0] = va->vertexs[0];
        new_vertexs[j*va->num_dimensions+1] = va->vertexs[1];
        if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[2];
        j++;
        new_vertexs[j*va->num_dimensions+0] = va->vertexs[(i-1)*va->num_dimensions+0];
        new_vertexs[j*va->num_dimensions+1] = va->vertexs[(i-1)*va->num_dimensions+1];
        if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[(i-1)*va->num_dimensions+2];
        j++;
      }
      new_vertexs[j*va->num_dimensions+0] = va->vertexs[i*va->num_dimensions+0];
      new_vertexs[j*va->num_dimensions+1] = va->vertexs[i*va->num_dimensions+1];
      if (va->num_dimensions == 3) new_vertexs[j*va->num_dimensions+2] = va->vertexs[i*va->num_dimensions+2];
      j++;
    }
    
    free(va->vertexs);
    out_shape->num_vertexs = new_num_vertexs;
    va->vertexs = new_vertexs;
    out_shape->gl_type = GL_TRIANGLES;
  }
  
  int i;
  for (i = 0 ; i < out_shape->num_vertexs ; i++)
  {
    float * v = get_vertex(out_shape, 0, i);
    append_vertex(final_shape, v);
  }
  free_shape(out_shape);
  out_shape = NULL;
}

void errorCallback(GLenum errorCode)
{
  const GLubyte *estring;

  estring = gluErrorString(errorCode);
  fprintf(stderr, "Tessellation Error: %s\n", estring);
  exit(1);
}

void combineCallback(GLdouble coords[3], 
                     GLdouble *vertex_data[4],
                     GLfloat weight[4], GLdouble **dataOut)
{
  GLdouble *vertex;
  int i;
  vertex = (GLdouble *) malloc(3 * sizeof(GLdouble));
  vertex[0] = coords[0];
  vertex[1] = coords[1];
  vertex[2] = coords[2];
  *dataOut = vertex;
}

int tesselate(int argc, char ** argv, FILE * pipe_in, FILE * pipe_out, FILE * pipe_err)
{
  GLUtesselator * tobj = NULL;
  tobj = gluNewTess();
  gluTessCallback(tobj, GLU_TESS_VERTEX, (GLvoid (*) ()) &vertex3dv);
  gluTessCallback(tobj, GLU_TESS_BEGIN, (GLvoid (*) ()) &beginCallback);
  gluTessCallback(tobj, GLU_TESS_END, (GLvoid (*) ()) &endCallback);
  gluTessCallback(tobj, GLU_TESS_ERROR, (GLvoid (*) ()) &errorCallback);
  gluTessCallback(tobj, GLU_TESS_COMBINE, (GLvoid (*) ()) &combineCallback);
  
  struct Shape * shape = NULL;
  long j=0, count=0;
  while ((shape = read_shape(pipe_in)))
  {
    final_shape = new_shape();
    final_shape->unique_set_id = shape->unique_set_id;
    final_shape->gl_type = GL_TRIANGLES;
    final_shape->vertex_arrays[0].num_dimensions = 2;
    
    int i;
    for (i = 0 ; i < shape->num_attributes ; i++)
      set_attribute(final_shape, shape->attributes[i].name, shape->attributes[i].value);
    
    if (shape->gl_type != GL_LINE_LOOP) { fprintf(pipe_err, "providing non line loop to tesselator. CANT DO IT\n"); exit(1); }
    gluTessBeginPolygon(tobj, NULL);
    gluTessBeginContour(tobj);
    
    if (shape->num_vertex_arrays != 1) { fprintf(pipe_err, "num_vertex_arrays != 1, CANT DO IT\n"); exit(1); }
    
    struct VertexArray * va = get_or_add_array(final_shape, GL_VERTEX_ARRAY);
    if (va->num_dimensions < 2 || va->num_dimensions > 3) { fprintf(pipe_err,"va->num_dimensions is %d", va->num_dimensions); continue; }
    for (j = 0 ; j < shape->num_vertexs ; j++)
    {
      GLdouble * vertex = (GLdouble*)malloc(3*sizeof(GLdouble));
      float * v = get_vertex(shape, 0, j);
      vertex[0] = v[0];
      vertex[1] = v[1];
      vertex[2] = 0;
      //if (va->num_dimensions == 3) vertex[2] = v[2];
      gluTessVertex(tobj, vertex, vertex);
    }
    
    gluTessEndContour(tobj);
    gluTessEndPolygon(tobj);
    
    count++;
    write_shape(pipe_out, final_shape);
    free_shape(final_shape);
  }
  gluDeleteTess(tobj);
  
  if (count == 0)
    fprintf(pipe_err, "There were no line loops to tesselate.\n");
  else
    fprintf(pipe_err, "Tesselator finished.\n");
}
