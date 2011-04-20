
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "shapefile_src/shapefil.h"

#define SCHEME_CREATE_MAIN
#define SCHEME_ASSERT_STDOUT_IS_PIPED
#define SCHEME_FUNCTION read_shapefile
#include "scheme.h"

int read_shapefile(int argc, char ** argv, FILE * pipe_in, FILE * pipe_out, FILE * pipe_err)
{
  char * filename = (argc > 1) ? argv[1] : "prov_ab_p_geo83_e.dbf";
  int row_id      = (argc > 2) ? atoi(argv[2]) : -1;
  int part_id     = (argc > 3) ? atoi(argv[3]) : -1;
  
  FILE * fp = fopen(filename, "r");
  if (!fp)
  {
    fprintf(stderr, "%s: error reading shapefile: %s\n", argv[0], filename);
    exit(1);
  }
  
  DBFHandle d = DBFOpen(filename, "rb");
  if (d == NULL) { fprintf(stderr, "DBFOpen error (%s.dbf)\n", filename); exit(1); }
	
	SHPHandle h = SHPOpen(filename, "rb");
  if (h == NULL) { fprintf(stderr, "SHPOpen error (%s.dbf)\n", filename); exit(1); }
	
  long nRecordCount = DBFGetRecordCount(d);
  long nFieldCount = DBFGetFieldCount(d);
  
  long t=0;
  long i;
  for (i = 0 ; i < nRecordCount ; i++)
  {
    if (row_id != -1 && row_id != i) continue;
    SHPObject	*psShape = SHPReadObject(h, i);
    if (psShape->nSHPType == SHPT_POINT)
    {
      struct Shape * shape = new_shape();
      shape->unique_set_id = t++;
      
      int j;
      for (j = 0 ; j < psShape->nVertices ; j++)
      {
        float v[2] = { psShape->padfX[j], psShape->padfY[j] };
        append_vertex(shape, v);
      }
      write_shape(pipe_out, shape);
      free_shape(shape);
    }
    else if (psShape->nSHPType == SHPT_ARC || psShape->nSHPType == SHPT_POLYGON)
    {
      //fprintf(stderr, "shape %d has %d parts and %d vertices\n", psShape->nShapeId, psShape->nParts, psShape->nVertices);
      
      if (psShape->nParts == 0) { fprintf(stderr, "polygon %d has 0 parts (thats bad)\n", psShape->nShapeId); return 0; }
      
      int j, iPart;
      for (iPart = 0 ; iPart < psShape->nParts ; iPart++)
      {
        if (part_id != -1 && part_id != iPart) continue;
        int start = psShape->panPartStart[iPart];
        int end = psShape->nVertices;
        
        if (iPart != psShape->nParts - 1)
        {
          end = psShape->panPartStart[iPart+1];
        }
        
        struct Shape * shape = new_shape();
        shape->unique_set_id = t++;
        
        if (psShape->nSHPType == SHPT_ARC)           shape->gl_type = GL_LINE_STRIP;
        else if (psShape->nSHPType == SHPT_POLYGON)  shape->gl_type = GL_LINE_LOOP;
        
        for (j = 0 ; j < nFieldCount ; j++)
        {
          char name[12];
          char value[50];
          int value_length;
          DBFFieldType field_type = DBFGetFieldInfo(d, j, name, &value_length, NULL);
          switch (field_type) {
            case FTString:
              set_attribute(shape, name, (char *)DBFReadStringAttribute(d, i, j));
              break;
            case FTInteger:
              sprintf(value, "%d", DBFReadIntegerAttribute(d, i, j));
              set_attribute(shape, name, value);
              break;
            case FTDouble:
              sprintf(value, "%f", DBFReadDoubleAttribute(d, i, j));
              set_attribute(shape, name, value);
              break;
          }
        }
        
        for (j = start ; j < end ; j++)
        {
          float v[2] = { psShape->padfX[j], psShape->padfY[j] };
          append_vertex(shape, v);
        }
        write_shape(pipe_out, shape);
        free_shape(shape);
      }
    }
    else
    {
      fprintf(stderr, "doesn't do nSHPType:%d yet.\n", psShape->nSHPType);
      return 0;
    }
    SHPDestroyObject(psShape);
  }
  DBFClose(d);
  SHPClose(h);
}
