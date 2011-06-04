
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SCHEME_CREATE_MAIN
#define SCHEME_ASSERT_STDINOUT_ARE_PIPED
#define SCHEME_FUNCTION reduce_by_bbox
#include "scheme.h"

int reduce_by_bbox(int argc, char ** argv, FILE * pipe_in, FILE * pipe_out, FILE * pipe_err)
{
  char filename[300] = "";
  int c;
  while ((c = getopt(argc, argv, "f:")) != -1)
  switch (c)
  {
    case 'f':
      strncpy(filename, optarg, 300);
      break;
    default:
      abort();
  }
  
  FILE * fp = fopen(filename, "r");
  
  struct Shape * bbox_shape = read_shape(fp);
  if (read_shape(fp)) fprintf(stderr, "Providing more then one bbox in '%s'.  All additional bbox shapes are ignored.\n", filename);
  fclose(fp);
  
  struct minmax {
    float min;
    float max;
  };
  struct minmax *bbox = NULL;
  
  struct VertexArray * va = get_or_add_array(bbox_shape, GL_VERTEX_ARRAY);
  
  int j,k;
  bbox = malloc(sizeof(struct minmax)*va->num_dimensions);
  for (j = 0 ; j < va->num_dimensions ; j++)
  {
    bbox[j].min =  1000000.0;
    bbox[j].max = -1000000.0;
  }
  
  for (j = 0 ; j < bbox_shape->num_vertexs ; j++)
  {
    for (k = 0 ; k < va->num_dimensions ; k++)
    {
      if (va->vertexs[j*va->num_dimensions+k] < bbox[k].min) bbox[k].min = va->vertexs[j*va->num_dimensions+k];
      if (va->vertexs[j*va->num_dimensions+k] > bbox[k].max) bbox[k].max = va->vertexs[j*va->num_dimensions+k];
    }
  }
  
  if (bbox_shape->num_vertexs != 4) fprintf(stderr, "Provided bbox has %d vertexs. (only 4 required).\n", bbox_shape->num_vertexs);
  if (bbox_shape->num_vertex_arrays != 1) fprintf(stderr, "Provided bbox has %d vertex arrays. (only 1 required).\n", bbox_shape->num_vertex_arrays);
  if (bbox_shape->gl_type != GL_LINE_LOOP) fprintf(stderr, "Provided bbox is not a GL_LINE_LOOP.\n", bbox_shape->gl_type);
  
  struct Shape * shape = NULL;
  while ((shape = read_shape(pipe_in)))
  {
    struct VertexArray * va = get_or_add_array(shape, GL_VERTEX_ARRAY);
    
    for (j = 0 ; j < shape->num_vertexs ; j++)
    {
      float * vertex = &va->vertexs[j*va->num_dimensions];
    
      if (vertex[0] < bbox[0].min || vertex[0] > bbox[0].max ||
          vertex[1] < bbox[1].min || vertex[1] > bbox[1].max)
      {
        j--;
        shape->num_vertexs--;
        memmove(vertex, vertex+va->num_dimensions, sizeof(float)*(shape->num_vertexs-j)*va->num_dimensions);
      }
    }
    
    if (shape->num_vertexs > 0)
      write_shape(pipe_out, shape);
    
    free_shape(shape);
  }
  
  //fprintf(stderr, "%s: %ld shapes reduced to %ld\n", argv[0], shapes_read, shapes_write);
}
